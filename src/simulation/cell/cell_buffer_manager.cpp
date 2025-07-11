#include "cell_buffer_manager.h"
#include "../../core/config.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CellBufferManager::CellBufferManager()
{
    // Generate sphere mesh - optimized for high cell counts
    sphereMesh.generateSphere(8, 12, 1.0f); // Ultra-low poly: 8x12 = 96 triangles for maximum performance
    sphereMesh.setupBuffers();

    initializeGPUBuffers();
}

CellBufferManager::~CellBufferManager()
{
    cleanup();
}

void CellBufferManager::cleanup()
{
    // Clean up triple buffered cell buffers
    for (int i = 0; i < 3; i++)
    {
        if (cellBuffer[i] != 0)
        {
            glDeleteBuffers(1, &cellBuffer[i]);
            cellBuffer[i] = 0;
        }
    }
    if (instanceBuffer != 0)
    {
        glDeleteBuffers(1, &instanceBuffer);
        instanceBuffer = 0;
    }
    if (modeBuffer != 0)
    {
        glDeleteBuffers(1, &modeBuffer);
        modeBuffer = 0;
    }
    if (gpuCellCountBuffer != 0)
    {
        glDeleteBuffers(1, &gpuCellCountBuffer);
        gpuCellCountBuffer = 0;
    }
    if (stagingCellCountBuffer != 0)
    {
        glDeleteBuffers(1, &stagingCellCountBuffer);
        stagingCellCountBuffer = 0;
    }
    if (stagingCellBuffer != 0)
    {
        glDeleteBuffers(1, &stagingCellBuffer);
        stagingCellBuffer = 0;
    }
    if (cellAdditionBuffer != 0)
    {
        glDeleteBuffers(1, &cellAdditionBuffer);
        cellAdditionBuffer = 0;
    }

    sphereMesh.cleanup();
}

void CellBufferManager::initializeGPUBuffers()
{
    // Create triple buffered compute buffers for cell data
    for (int i = 0; i < 3; i++)
    {
        std::vector<ComputeCell> zeroCells(cellLimit);
        glCreateBuffers(1, &cellBuffer[i]);
        glNamedBufferData(
            cellBuffer[i],
            cellLimit * sizeof(ComputeCell),
            zeroCells.data(),
            GL_DYNAMIC_COPY  // Used by both GPU compute and CPU read operations
        );
    }

    // Create instance buffer for rendering (contains position + radius + color + orientation)
    glCreateBuffers(1, &instanceBuffer);
    glNamedBufferData(
        instanceBuffer,
        cellLimit * sizeof(glm::vec4) * 3, // 3 vec4s: positionAndRadius, color, orientation
        nullptr,
        GL_DYNAMIC_COPY  // GPU produces data, GPU consumes for rendering
    );

    // Create single buffered genome buffer
    glCreateBuffers(1, &modeBuffer);
    glNamedBufferData(modeBuffer,
        cellLimit * sizeof(GPUMode),
        nullptr,
        GL_DYNAMIC_COPY  // Written once by CPU, read frequently by GPU compute shaders
    );

    // A buffer that keeps track of how many cells there are in the simulation
    glCreateBuffers(1, &gpuCellCountBuffer);
    glNamedBufferData(gpuCellCountBuffer,
        sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Create staging buffer for cell count (CPU-accessible, no sync stalls)
    glCreateBuffers(1, &stagingCellCountBuffer);
    glNamedBufferData(stagingCellCountBuffer,
        sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Create staging buffer for cell data (CPU-accessible, no sync stalls)
    glCreateBuffers(1, &stagingCellBuffer);
    glNamedBufferData(stagingCellBuffer,
        cellLimit * sizeof(ComputeCell),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Create cell addition buffer for GPU
    glCreateBuffers(1, &cellAdditionBuffer);
    glNamedBufferData(cellAdditionBuffer,
        cellLimit * sizeof(ComputeCell),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Map the staging buffers for CPU access
    mappedPtr = glMapNamedBuffer(stagingCellCountBuffer, GL_READ_WRITE);
    countPtr = static_cast<GLuint*>(mappedPtr);
    *countPtr = 0;

    mappedCellPtr = glMapNamedBuffer(stagingCellBuffer, GL_READ_WRITE);
}

void CellBufferManager::resetSimulation()
{
    // Reset cell count
    *countPtr = 0;
    cellCount = 0;
    cpuPendingCellCount = 0;
    gpuPendingCellCount = 0;
    adhesionCount = 0;

    // Clear all cell buffers
    std::vector<ComputeCell> zeroCells(cellLimit);
    for (int i = 0; i < 3; i++)
    {
        glNamedBufferSubData(cellBuffer[i], 0, cellLimit * sizeof(ComputeCell), zeroCells.data());
    }

    // Clear instance buffer
    glNamedBufferSubData(instanceBuffer, 0, cellLimit * sizeof(glm::vec4) * 3, nullptr);

    // Clear cell addition buffer
    glNamedBufferSubData(cellAdditionBuffer, 0, cellLimit * sizeof(ComputeCell), nullptr);

    // Reset buffer rotation
    bufferRotation = 0;
}

void CellBufferManager::addCellsToGPUBuffer(const std::vector<ComputeCell> &cells)
{
    if (cells.empty()) return;

    // Add cells to staging buffer first
    for (const auto& cell : cells)
    {
        addCellToStagingBuffer(cell);
    }

    // Apply staged cells to GPU buffer
    addStagedCellsToGPUBuffer();
}

void CellBufferManager::addCellToGPUBuffer(const ComputeCell &newCell)
{
    // Add to staging buffer first
    addCellToStagingBuffer(newCell);
    
    // Apply staged cells to GPU buffer
    addStagedCellsToGPUBuffer();
}

void CellBufferManager::addCellToStagingBuffer(const ComputeCell &newCell)
{
    if (cpuPendingCellCount >= cellLimit)
    {
        std::cout << "Warning: Cell limit reached, cannot add more cells" << std::endl;
        return;
    }

    cellStagingBuffer.push_back(newCell);
    cpuPendingCellCount++;
}

void CellBufferManager::addStagedCellsToGPUBuffer()
{
    if (cellStagingBuffer.empty()) return;

    // Copy staged cells to GPU addition buffer
    glNamedBufferSubData(cellAdditionBuffer, 0, 
        cellStagingBuffer.size() * sizeof(ComputeCell), 
        cellStagingBuffer.data());

    // Update cell count
    cellCount += static_cast<int>(cellStagingBuffer.size());
    gpuPendingCellCount += static_cast<int>(cellStagingBuffer.size());

    // Clear staging buffer
    cellStagingBuffer.clear();
    cpuPendingCellCount = 0;
}

void CellBufferManager::addGenomeToBuffer(GenomeData& genomeData) const
{
    std::vector<GPUMode> gpuModes;
    gpuModes.reserve(genomeData.modes.size());

    for (const auto& mode : genomeData.modes)
    {
        GPUMode gpuMode;
        gpuMode.color = glm::vec4(mode.color, 1.0f);
        
        // Convert pitch/yaw to quaternions
        float pitchRad = glm::radians(mode.parentSplitDirection.x);
        float yawRad = glm::radians(mode.parentSplitDirection.y);
        
        gpuMode.orientationA = glm::quat(glm::vec3(pitchRad, yawRad, 0.0f));
        gpuMode.orientationB = glm::quat(glm::vec3(-pitchRad, yawRad + M_PI, 0.0f));
        
        gpuMode.splitDirection = glm::vec4(
            cos(yawRad) * cos(pitchRad),
            sin(yawRad) * cos(pitchRad),
            sin(pitchRad),
            0.0f
        );
        
        gpuMode.childModes = glm::ivec2(mode.childA.modeNumber, mode.childB.modeNumber);
        gpuMode.splitInterval = mode.splitInterval;
        gpuMode.genomeOffset = 0; // Not used in current implementation
        gpuMode.adhesionSettings = mode.adhesionSettings;
        gpuMode.parentMakeAdhesion = mode.parentMakeAdhesion ? 1 : 0;
        
        gpuModes.push_back(gpuMode);
    }

    // Upload to GPU
    glNamedBufferSubData(modeBuffer, 0, gpuModes.size() * sizeof(GPUMode), gpuModes.data());
}

void CellBufferManager::applyCellAdditions()
{
    if (gpuPendingCellCount <= 0) return;

    // This function should be called by the compute shader system
    // The actual addition logic is handled in the compute shader
    gpuPendingCellCount = 0;
}

ComputeCell CellBufferManager::getCellData(int index) const
{
    if (index < 0 || index >= cellLimit) {
        return ComputeCell{};
    }

    // Read from staging buffer (CPU-accessible)
    ComputeCell cell;
    glGetNamedBufferSubData(stagingCellBuffer, 
        index * sizeof(ComputeCell), 
        sizeof(ComputeCell), 
        &cell);
    
    return cell;
}

void CellBufferManager::updateCellData(int index, const ComputeCell &newData)
{
    if (index < 0 || index >= cellLimit) {
        return;
    }

    // Update staging buffer (CPU-accessible)
    glNamedBufferSubData(stagingCellBuffer, 
        index * sizeof(ComputeCell), 
        sizeof(ComputeCell), 
        &newData);

    // Also update the current read buffer for immediate effect
    // Note: This follows the buffer access rules - we're updating the staging buffer
    // which is separate from the triple-buffered compute buffers
}

void CellBufferManager::syncCellPositionsFromGPU()
{
    // Copy current GPU cell data to staging buffer for CPU access
    glCopyNamedBufferSubData(
        getCellReadBuffer(),  // Source: current read buffer
        stagingCellBuffer,    // Destination: staging buffer
        0, 0,                 // Source and destination offsets
        cellLimit * sizeof(ComputeCell)  // Size to copy
    );
}

void CellBufferManager::restoreCellsDirectlyToGPUBuffer(const std::vector<ComputeCell> &cells)
{
    if (cells.empty()) return;

    // Update cell count
    cellCount = static_cast<int>(cells.size());
    *countPtr = static_cast<GLuint>(cells.size());

    // Copy cells to all three buffers to ensure consistency
    for (int i = 0; i < 3; i++)
    {
        glNamedBufferSubData(cellBuffer[i], 0, 
            cells.size() * sizeof(ComputeCell), 
            cells.data());
    }

    // Also update staging buffer
    glNamedBufferSubData(stagingCellBuffer, 0, 
        cells.size() * sizeof(ComputeCell), 
        cells.data());

    // Reset buffer rotation
    bufferRotation = 0;
}

void CellBufferManager::setCPUCellData(const std::vector<ComputeCell> &cells)
{
    cpuCells = cells;
    cellCount = static_cast<int>(cells.size());
} 