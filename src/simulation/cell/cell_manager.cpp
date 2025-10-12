#include "cell_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../core/config.h"
#include "../../ui/ui_manager.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <fstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../../utils/timer.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

CellManager::CellManager()
{
    // Generate sphere mesh - optimized for high cell counts
    sphereMesh.generateSphere(8, 12, 1.0f); // Ultra-low poly: 8x12 = 96 triangles for maximum performance
    sphereMesh.setupBuffers();

    initializeGPUBuffers();
    initializeSpatialGrid();

    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell/physics/cell_physics_spatial.comp"); // Use spatial partitioning version
    positionUpdateShader = new Shader("shaders/cell/physics/cell_position_update.comp");
    velocityUpdateShader = new Shader("shaders/cell/physics/cell_velocity_update.comp");
    internalUpdateShader = new Shader("shaders/cell/physics/cell_update_internal.comp");
    extractShader = new Shader("shaders/cell/management/extract_instances.comp");
    cellAdditionShader = new Shader("shaders/cell/management/apply_additions.comp");

    // Initialize spatial grid shaders
    gridClearShader = new Shader("shaders/spatial/grid_clear.comp");
    gridAssignShader = new Shader("shaders/spatial/grid_assign.comp");
    gridPrefixSumShader = new Shader("shaders/spatial/grid_prefix_sum.comp");
    gridInsertShader = new Shader("shaders/spatial/grid_insert.comp");
    
    // Initialize gizmo shaders
    gizmoExtractShader = new Shader("shaders/rendering/debug/gizmo_extract.comp");
    gizmoShader = new Shader("shaders/rendering/debug/gizmo.vert", "shaders/rendering/debug/gizmo.frag");
    
    // Initialize ring gizmo shaders
    ringGizmoExtractShader = new Shader("shaders/rendering/debug/ring_gizmo_extract.comp");
    ringGizmoShader = new Shader("shaders/rendering/debug/ring_gizmo.vert", "shaders/rendering/debug/ring_gizmo.frag");
    
    // Initialize anchor gizmo shaders
    anchorGizmoExtractShader = new Shader("shaders/rendering/debug/anchor_gizmo_extract.comp");
    anchorGizmoShader = new Shader("shaders/rendering/debug/anchor_gizmo.vert", "shaders/rendering/debug/anchor_gizmo.frag");
    
    // Initialize adhesionSettings line shaders
    adhesionLineExtractShader = new Shader("shaders/rendering/debug/adhesion_line_extract.comp");
    adhesionLineShader = new Shader("shaders/rendering/debug/adhesion_line.vert", "shaders/rendering/debug/adhesion_line.frag");
    
    // Initialize adhesionSettings physics  shader
    adhesionPhysicsShader = new Shader("shaders/cell/physics/adhesion_physics.comp");
    
    // Initialize gizmo buffers
    initializeGizmoBuffers();
    initializeRingGizmoBuffers();
    initializeAnchorGizmoBuffers();
    initializeAdhesionLineBuffers();
    initializeAdhesionConnectionSystem();
    
    // Initialize LOD system
    initializeLODSystem();
    
    // Initialize unified culling system
    initializeUnifiedCulling();
    
    // Initialize barrier optimization system
    barrierBatch.setStats(&barrierStats);
    
    // Load global flagellocyte settings
    loadGlobalFlagellocyteSettings();
    
    // Initialize particle system
    initializeParticleSystem();
}

CellManager::~CellManager()
{
    cleanup();
}

// ============================================================================
// CLEANUP
// ============================================================================

void CellManager::cleanup()
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
    if (freeCellSlotBuffer != 0)
    {
        glDeleteBuffers(1, &freeCellSlotBuffer);
        freeCellSlotBuffer = 0;
    }
    if (freeAdhesionSlotBuffer != 0)
    {
        glDeleteBuffers(1, &freeAdhesionSlotBuffer);
        freeAdhesionSlotBuffer = 0;
    }
    if (uniqueIdBuffer != 0)
    {
        glDeleteBuffers(1, &uniqueIdBuffer);
        uniqueIdBuffer = 0;
    }

    cleanupSpatialGrid();
    cleanupLODSystem();
    cleanupUnifiedCulling();

    if (extractShader)
    {
        extractShader->destroy();
        delete extractShader;
        extractShader = nullptr;
    }
    if (physicsShader)
    {
        physicsShader->destroy();
        delete physicsShader;
        physicsShader = nullptr;
    }
    if (positionUpdateShader)
    {
        positionUpdateShader->destroy();
        delete positionUpdateShader;
        positionUpdateShader = nullptr;
    }

    // Cleanup spatial grid shaders
    if (gridClearShader)
    {
        gridClearShader->destroy();
        delete gridClearShader;
        gridClearShader = nullptr;
    }
    if (gridAssignShader)
    {
        gridAssignShader->destroy();
        delete gridAssignShader;
        gridAssignShader = nullptr;
    }
    if (gridPrefixSumShader)
    {
        gridPrefixSumShader->destroy();
        delete gridPrefixSumShader;
        gridPrefixSumShader = nullptr;
    }
    if (gridInsertShader)
    {
        gridInsertShader->destroy();
        delete gridInsertShader;
        gridInsertShader = nullptr;
    }
    
    // Cleanup gizmo shaders
    if (gizmoExtractShader)
    {
        gizmoExtractShader->destroy();
        delete gizmoExtractShader;
        gizmoExtractShader = nullptr;
    }
    if (gizmoShader)
    {
        gizmoShader->destroy();
        delete gizmoShader;
        gizmoShader = nullptr;
    }
    if (ringGizmoExtractShader)
    {
        ringGizmoExtractShader->destroy();
        delete ringGizmoExtractShader;
        ringGizmoExtractShader = nullptr;
    }
    if (anchorGizmoExtractShader)
    {
        anchorGizmoExtractShader->destroy();
        delete anchorGizmoExtractShader;
        anchorGizmoExtractShader = nullptr;
    }
    if (ringGizmoShader)
    {
        ringGizmoShader->destroy();
        delete ringGizmoShader;
        ringGizmoShader = nullptr;
    }

    if (anchorGizmoShader)
    {
        anchorGizmoShader->destroy();
        delete anchorGizmoShader;
        anchorGizmoShader = nullptr;
    }

    if (adhesionLineShader)
    {
        adhesionLineShader->destroy();
        delete adhesionLineShader;
        adhesionLineShader = nullptr;
    }

    if (adhesionPhysicsShader)
    {
        adhesionPhysicsShader->destroy();
        delete adhesionPhysicsShader;
        adhesionPhysicsShader = nullptr;
    }
    
    cleanupGizmos();
    cleanupRingGizmos();
    cleanupAnchorGizmos();
    cleanupAdhesionLines();
    cleanupLODSystem();
    cleanupParticleSystem();
    sphereMesh.cleanup();
}

// ============================================================================
// BUFFER MANAGEMENT
// ============================================================================

void CellManager::initializeGPUBuffers()
{    // Create triple buffered compute buffers for cell data
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

    // Create free slot buffers for storing indices of dead cells and adhesions
    glCreateBuffers(1, &freeCellSlotBuffer);
    glNamedBufferData(
        freeCellSlotBuffer,
        cellLimit * sizeof(int),
        nullptr,
        GL_DYNAMIC_COPY  // GPU produces data, GPU consumes for rendering
    );
    glCreateBuffers(1, &freeAdhesionSlotBuffer);
    glNamedBufferData(
        freeAdhesionSlotBuffer,
        cellLimit * config::MAX_ADHESIONS_PER_CELL * sizeof(int) / 2,
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
    glNamedBufferStorage(
        gpuCellCountBuffer,
        sizeof(GLuint) * config::COUNTER_NUMBER, // stores current cell counts and adhesion counts
        nullptr,
        GL_DYNAMIC_STORAGE_BIT
    );
    glCreateBuffers(1, &stagingCellCountBuffer);
    glNamedBufferStorage(
        stagingCellCountBuffer,
        sizeof(GLuint) * config::COUNTER_NUMBER,
        nullptr,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT
    );
    mappedPtr = glMapNamedBufferRange(stagingCellCountBuffer, 0, sizeof(GLuint) * config::COUNTER_NUMBER,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    countPtr = static_cast<GLuint*>(mappedPtr);

    // Cell data staging buffer for CPU reads (avoids GPU->CPU transfer warnings)
    glCreateBuffers(1, &stagingCellBuffer);
    glNamedBufferStorage(
        stagingCellBuffer,
        cellLimit * sizeof(ComputeCell),
        nullptr,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT
    );
    mappedCellPtr = glMapNamedBufferRange(stagingCellBuffer, 0, cellLimit * sizeof(ComputeCell),
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    // Create unique ID counter buffer
    glCreateBuffers(1, &uniqueIdBuffer);
    glNamedBufferStorage(
        uniqueIdBuffer,
        sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_STORAGE_BIT
    );
    // Initialize with 1 (0 reserved for root cells)
    GLuint initialId = 1;
    glNamedBufferSubData(uniqueIdBuffer, 0, sizeof(GLuint), &initialId);

    // Cell addition queue buffer - FIXED: Increased size for 100k cells
    glCreateBuffers(1, &cellAdditionBuffer);
    glNamedBufferData(cellAdditionBuffer,
        cellLimit * sizeof(ComputeCell), // Full size to handle large simultaneous splits
        nullptr,
        GL_STREAM_COPY  // Frequently updated by GPU compute shaders
    );

    // Setup the sphere mesh to use our current instance buffer
    sphereMesh.setupInstanceBuffer(instanceBuffer);

    // Reserve CPU storage
    cpuCells.reserve(cellLimit);
}

// ============================================================================
// CELL ADDITION & QUEUE MANAGEMENT
// ============================================================================

void CellManager::addCellsToQueueBuffer(const std::vector<ComputeCell> &cells)
{ // Prefer to not use this directly, use addCellToStagingBuffer instead
    int newCellCount = static_cast<int>(cells.size());

    if (totalCellCount + newCellCount > cellLimit)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    TimerGPU gpuTimer("Adding Cells to GPU Buffers");

    glNamedBufferSubData(cellAdditionBuffer,
        0,
        newCellCount * sizeof(ComputeCell),
        cells.data());
}

void CellManager::addCellToStagingBuffer(const ComputeCell &newCell)
{
    if (totalCellCount + 1 > cellLimit)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    // Create a copy of the cell and enforce radius = 1.0f
    ComputeCell correctedCell = newCell;
    correctedCell.positionAndMass.w = 1.0f; // Force all cells to have radius of 1

    // Add to CPU storage only (no immediate GPU sync)
    cellStagingBuffer.push_back(correctedCell);
    cpuCells.push_back(correctedCell);
    pendingCellCount++;
}

void CellManager::addStagedCellsToQueueBuffer()
{
    if (cellStagingBuffer.empty()) return;
    addCellsToQueueBuffer(cellStagingBuffer);
    cellStagingBuffer.clear(); // Clear after adding to GPU buffer

    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    applyCellAdditions(); // Add the cells from gpu queue buffer to main cell buffers

    // CRITICAL FIX: Update CPU-side cell count to match GPU after adding cells
    updateCounts();

    pendingCellCount = 0;      // Reset pending count
}

void CellManager::restoreCellsDirectlyToGPUBuffer(const std::vector<ComputeCell> &cells)
{
    // This function is specifically for keyframe restoration
    // It bypasses the addition buffer system and writes directly to main GPU buffers
    
    int newCellCount = static_cast<int>(cells.size());
    
    if (newCellCount > cellLimit) {
        std::cout << "Warning: Restoration cell count exceeds limit!\n";
        return;
    }
    
    if (newCellCount == 0) {
        return;
    }
    
    TimerGPU gpuTimer("Restoring Cells Directly to GPU Buffers");
    
    // Update main cell buffers directly (both current and previous for consistency)
    for (int i = 0; i < 3; i++) { // Update all 3 buffers for proper rotation
        glNamedBufferSubData(cellBuffer[i],
                             0, // Start from beginning
                             newCellCount * sizeof(ComputeCell),
                             cells.data());
    }
    
    // Update cell count directly
    totalCellCount = newCellCount;
    GLuint counts[config::COUNTER_NUMBER] = { static_cast<GLuint>(totalCellCount), static_cast<GLuint>(totalAdhesionCount), static_cast<GLuint>(pendingCellCount) }; // totalCellCount, totalAdhesionCount, pendingCellCount
    glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint) * config::COUNTER_NUMBER, counts);
    
    // Sync staging buffer
    syncCounterBuffers();
    
    // Clear addition buffer since we're not using it
    glClearNamedBufferData(cellAdditionBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    
    // Ensure GPU buffers are synchronized before proceeding
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::setCPUCellData(const std::vector<ComputeCell> &cells)
{
    // This function updates the CPU cell storage to match restored GPU data
    cpuCells.clear();
    cpuCells.reserve(cells.size());
    
    for (const auto& cell : cells) {
        cpuCells.push_back(cell);
    }
    
    totalCellCount = static_cast<int>(cells.size());
    pendingCellCount = 0;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

glm::vec3 pitchYawToVec3(float pitch, float yaw) {
    return glm::vec3(
        cos(pitch) * sin(yaw),
        sin(pitch),
        cos(pitch) * cos(yaw)
    );
}
// GENOME & MODE MANAGEMENT
// ============================================================================

void CellManager::addGenomeToBuffer(GenomeData& genomeData) {
    // Update the cell manager's copy of the genome for rendering
    currentGenome = genomeData;
    
    int genomeBaseOffset = 0; // Later make it add to the end of the buffer
    int modeCount = static_cast<int>(genomeData.modes.size());

    std::vector<GPUMode> gpuModes;
    gpuModes.reserve(modeCount);

    for (size_t i = 0; i < modeCount; ++i) {
        const ModeSettings& mode = genomeData.modes[i];

        GPUMode gmode{};
        gmode.color = glm::vec4(mode.color, 1.0); // Set alpha to 1.0 instead of 0.0
        
        gmode.splitInterval = mode.splitInterval;
        gmode.genomeOffset = genomeBaseOffset;

        // Convert from pitch and yaw to padded vec4
        gmode.splitDirection = glm::vec4(pitchYawToVec3(
            glm::radians(mode.parentSplitDirection.x), glm::radians(mode.parentSplitDirection.y)), 0.);

        // Store child mode indices with bounds checking to prevent invalid mode references
        {
            int clampedChildA = mode.childA.modeNumber;
            int clampedChildB = mode.childB.modeNumber;
            if (clampedChildA < 0 || clampedChildA >= modeCount) {
                std::cout << "WARNING: Child A mode index out of range (" << clampedChildA
                          << ") clamping to [0," << (modeCount - 1) << "]\n";
                clampedChildA = std::max(0, std::min(clampedChildA, modeCount - 1));
            }
            if (clampedChildB < 0 || clampedChildB >= modeCount) {
                std::cout << "WARNING: Child B mode index out of range (" << clampedChildB
                          << ") clamping to [0," << (modeCount - 1) << "]\n";
                clampedChildB = std::max(0, std::min(clampedChildB, modeCount - 1));
            }
            gmode.childModes = glm::ivec2(clampedChildA, clampedChildB);
        }

        // Directly store quaternions (no conversion)
        gmode.orientationA = mode.childA.orientation;  // now a quat already
        gmode.orientationB = mode.childB.orientation;
        
        // Store adhesionSettings flag
        gmode.parentMakeAdhesion = mode.parentMakeAdhesion;
        gmode.childAKeepAdhesion = mode.childA.keepAdhesion;
        gmode.childBKeepAdhesion = mode.childB.keepAdhesion;
        gmode.maxAdhesions = mode.maxAdhesions;
        
        // Store flagellocyte thrust force
        gmode.flagellocyteThrustForce = (mode.cellType == CellType::Flagellocyte) ? 
            mode.flagellocyteSettings.thrustForce : 0.0f;

        // Store adhesion settings (convert bools to ints and pack for GPU)
        gmode.adhesionSettings.canBreak = mode.adhesionSettings.canBreak ? 1 : 0;
        gmode.adhesionSettings.breakForce = mode.adhesionSettings.breakForce;
        gmode.adhesionSettings.restLength = mode.adhesionSettings.restLength;
        gmode.adhesionSettings.linearSpringStiffness = mode.adhesionSettings.linearSpringStiffness;
        gmode.adhesionSettings.linearSpringDamping = mode.adhesionSettings.linearSpringDamping;
        gmode.adhesionSettings.orientationSpringStiffness = mode.adhesionSettings.orientationSpringStiffness;
        gmode.adhesionSettings.orientationSpringDamping = mode.adhesionSettings.orientationSpringDamping;
        gmode.adhesionSettings.maxAngularDeviation = mode.adhesionSettings.maxAngularDeviation;
        gmode.adhesionSettings.twistConstraintStiffness = mode.adhesionSettings.twistConstraintStiffness;
        gmode.adhesionSettings.twistConstraintDamping = mode.adhesionSettings.twistConstraintDamping;
        gmode.adhesionSettings.enableTwistConstraint = mode.adhesionSettings.enableTwistConstraint ? 1 : 0;

        gpuModes.push_back(gmode);
    }

    glNamedBufferSubData(
        modeBuffer,
        genomeBaseOffset,
        modeCount * sizeof(GPUMode),
        gpuModes.data()
    );
    
    // CRITICAL FIX: Add memory barrier to ensure GPU sees updated mode buffer data
    // This prevents cells from appearing black when colors are changed
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
}

// ============================================================================
// CELL DATA ACCESS & MODIFICATION
// ============================================================================

ComputeCell CellManager::getCellData(int index) const
{
    if (index >= 0 && index < totalCellCount && index < static_cast<int>(cpuCells.size()))
    {
        return cpuCells[index];
    }
    ComputeCell emptyCell{};
    return emptyCell; // Return empty cell if index is invalid
}

void CellManager::updateCellData(int index, const ComputeCell &newData)
{
    if (index >= 0 && index < totalCellCount)
    {
        cpuCells[index] = newData;

        // Update selected cell cache if this is the selected cell
        if (selectedCell.isValid && selectedCell.cellIndex == index)
        {
            selectedCell.cellData = newData;
        }

        // Update the specific cell in both GPU buffers to keep them synchronized
        for (int i = 0; i < 2; i++)
        {
            glNamedBufferSubData(cellBuffer[i],
                                 index * sizeof(ComputeCell),
                                 sizeof(ComputeCell),
                                 &cpuCells[index]);
        }
    }
}

std::string CellManager::getCellLineageString(int index) const
{
    if (index >= 0 && index < totalCellCount && index < static_cast<int>(cpuCells.size()))
    {
        return cpuCells[index].getLineageString();
    }
    return "Invalid";
}

// ============================================================================
// CELL UPDATE & SIMULATION
// ============================================================================

void CellManager::applyForces(float deltaTime)
{
    // Run physics computation on GPU
    runCollisionCompute();

    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    runAdhesionPhysics(deltaTime);

    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::verletIntegration(float deltaTime)
{
    // Update spatial grid before physics
    //updateSpatialGrid(); // This handles its own barriers internally

    //addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    //applyForces(deltaTime);

    runPositionUpdateCompute(deltaTime);

    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Update spatial grid because cells have moved
    updateSpatialGrid(); // This handles its own barriers internally

    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

	applyForces(deltaTime);

    runVelocityUpdateCompute(deltaTime);

    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::updateCells(float deltaTime)
{
    // Increment frame counter
    currentFrame++;
    
    // Clear any pending barriers from previous frame
    clearBarriers();

    if (pendingCellCount > 0)
    {
        addStagedCellsToQueueBuffer(); // Sync any pending cells to GPU
    }

    int previousCellCount = totalCellCount;
    updateCounts();
    
    // CRITICAL FIX: Safety check to prevent cell count overflow
    if (totalCellCount > cellLimit) {
        std::cout << "Warning: Cell count exceeded limit! Clamping to " << cellLimit << std::endl;
        totalCellCount = cellLimit;
        liveCellCount = std::min(liveCellCount, cellLimit);
        
        // Update GPU buffer with clamped values
        GLuint counts[4] = { 
            static_cast<GLuint>(totalCellCount), 
            static_cast<GLuint>(liveCellCount), 
            static_cast<GLuint>(totalAdhesionCount), 
            static_cast<GLuint>(liveAdhesionCount) 
        };
        glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint) * 4, counts);
    }
    
    // Invalidate cache if cell count changed (affects legacy calculation)
    if (previousCellCount != totalCellCount) {
        invalidateStatisticsCache();
    }

    if (totalCellCount > 0) // Don't update cells if there are no cells to update
    {
        // Flush barriers before starting compute pipeline
        flushBarriers();

        verletIntegration(deltaTime);

        // Run cells' internal calculations (this creates new pending cells from mitosis)
        runInternalUpdateCompute(deltaTime);
        
        // CRITICAL FIX: Add memory barrier after splitting to ensure new cells are synchronized
        addBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }
}

void CellManager::updateCellsFastForward(float deltaTime)
{
    // Increment frame counter
    currentFrame++;
    
    // Clear any pending barriers from previous frame
    clearBarriers();

    if (pendingCellCount > 0)
    {
        addStagedCellsToQueueBuffer(); // Sync any pending cells to GPU
    }

    int previousCellCount = totalCellCount;
    updateCounts();
    
    // CRITICAL FIX: Safety check to prevent cell count overflow
    if (totalCellCount > cellLimit) {
        totalCellCount = cellLimit;
        liveCellCount = std::min(liveCellCount, cellLimit);
        
        // Update GPU buffer with clamped values
        GLuint counts[4] = { 
            static_cast<GLuint>(totalCellCount), 
            static_cast<GLuint>(liveCellCount), 
            static_cast<GLuint>(totalAdhesionCount), 
            static_cast<GLuint>(liveAdhesionCount) 
        };
        glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint) * 4, counts);
    }
    
    // Invalidate cache if cell count changed
    if (previousCellCount != totalCellCount) {
        invalidateStatisticsCache();
    }

    if (totalCellCount > 0) // Don't update cells if there are no cells to update
    {
        // Flush barriers before starting compute pipeline
        flushBarriers();

        verletIntegration(deltaTime);

        // Run cells' internal calculations
        runInternalUpdateCompute(deltaTime);
        
        // CRITICAL FIX: Add memory barrier after splitting to ensure new cells are synchronized
        addBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }
}

// ============================================================================
// FRAME SKIPPING OPTIMIZATION FOR RESIMULATION
// ============================================================================

CellManager::SimulationState CellManager::captureSimulationState()
{
    SimulationState state;
    state.cellCount = totalCellCount;
    state.adhesionCount = totalAdhesionCount;
    state.isValid = true;
    
    if (totalCellCount == 0) {
        return state;
    }
    
    // Sync cell data from GPU (lightweight operation)
    syncCellPositionsFromGPU();
    
    // Calculate aggregate metrics
    state.totalAge = 0.0f;
    state.centerOfMass = glm::vec3(0.0f);
    state.totalVelocity = 0.0f;
    
    for (int i = 0; i < totalCellCount && i < cpuCells.size(); i++) {
        const ComputeCell& cell = cpuCells[i];
        state.totalAge += cell.age;
        state.centerOfMass += glm::vec3(cell.positionAndMass);
        state.totalVelocity += glm::length(glm::vec3(cell.velocity));
    }
    
    if (totalCellCount > 0) {
        state.centerOfMass /= static_cast<float>(totalCellCount);
    }
    
    return state;
}

bool CellManager::canSkipFrame(const SimulationState& newState)
{
    if (!enableFrameSkipping || !previousSimState.isValid) {
        return false;
    }
    
    // Check if states are identical
    return newState == previousSimState;
}

int CellManager::updateCellsFastForwardOptimized(float timeToSimulate, float timeStep)
{
    int totalFrames = static_cast<int>(timeToSimulate / timeStep);
    int framesSkipped = 0;
    float timeRemaining = timeToSimulate;
    
    // Reset frame skipping state
    consecutiveIdenticalFrames = 0;
    previousSimState.isValid = false;
    
    while (timeRemaining > 0.0f) {
        float stepTime = (timeRemaining > timeStep) ? timeStep : timeRemaining;
        
        // Simulate one frame
        updateCellsFastForward(stepTime);
        
        // Capture new simulation state
        SimulationState newState = captureSimulationState();
        
        // Check if we can skip frames
        if (canSkipFrame(newState)) {
            consecutiveIdenticalFrames++;
            
            // If we've detected multiple identical frames, skip ahead
            if (consecutiveIdenticalFrames >= 3) {
                // Calculate how much time we can skip
                // Skip in chunks, checking periodically in case something changes
                int maxSkipFrames = std::min(50, static_cast<int>(timeRemaining / timeStep));
                
                if (maxSkipFrames > 0) {
                    float skipTime = maxSkipFrames * timeStep;
                    timeRemaining -= skipTime;
                    framesSkipped += maxSkipFrames;
                    
                    // Update cell ages during skip (cells are aging even when static)
                    for (int i = 0; i < totalCellCount && i < cpuCells.size(); i++) {
                        cpuCells[i].age += skipTime;
                    }
                    
                    // Sync updated ages back to GPU
                    if (totalCellCount > 0) {
                        glNamedBufferSubData(cellBuffer[bufferRotation], 0, 
                                           totalCellCount * sizeof(ComputeCell), 
                                           cpuCells.data());
                    }
                    
                    // Recapture state after age update
                    newState = captureSimulationState();
                }
            }
        } else {
            consecutiveIdenticalFrames = 0;
        }
        
        previousSimState = newState;
        timeRemaining -= stepTime;
    }
    
    return framesSkipped;
}

// ============================================================================
// COMPUTE SHADER DISPATCH
// ============================================================================

void CellManager::renderCells(glm::vec2 resolution, Shader &cellShader, Camera &camera, bool wireframe)
{
    // Use unified culling system if any culling is enabled
    if (useFrustumCulling || useDistanceCulling || useLODSystem) {
        renderCellsUnified(resolution, camera, wireframe);
        renderSphereSkin(camera, resolution);
        return;
    }
    
    if (totalCellCount == 0)
        return;

    // Safety check for zero-sized framebuffer (minimized window)
    if (resolution.x <= 0 || resolution.y <= 0)
    {
        return;
    }

    // Additional safety check for very small resolutions that might cause issues
    if (resolution.x < 1 || resolution.y < 1)
    {
        return;
    }
    try
    {
        // Calculate aspect ratio for frustum culling
        float aspectRatio = resolution.x / resolution.y;
        if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
        {
            aspectRatio = 16.0f / 9.0f;
        }
        
        // Use unified culling if enabled, otherwise fall back to regular extraction
        if (useFrustumCulling || useDistanceCulling || useLODSystem) {
            // Update frustum and perform unified culling
            	updateFrustum(camera, config::defaultFrustumFov, aspectRatio, config::defaultFrustumNearPlane, config::defaultFrustumFarPlane);
            runUnifiedCulling(camera);
            
            // Setup sphere mesh to use the unified output buffer
            sphereMesh.setupInstanceBuffer(unifiedOutputBuffers[0]);
        } else {
            // Use compute shader to efficiently extract instance data (original method)
            TimerGPU timer("Instance extraction");

            extractShader->use();

            // Bind current buffers for compute shader (read from current cell buffer, write to current instance buffer)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceBuffer); // Dispatch extract compute shader
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer); // Bind GPU cell count buffer
            GLuint numGroups = (totalCellCount + 255) / 256; // Updated to 256 for consistency
            extractShader->dispatch(numGroups, 1, 1);
            
            // Add barrier for instance extraction but don't flush yet
            addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
            
            // Setup sphere mesh to use the regular instance buffer
            sphereMesh.setupInstanceBuffer(instanceBuffer);
        }

        TimerGPU timer("Cell Rendering");
        
        // Flush barriers before rendering to ensure instance data is ready
        flushBarriers();

        // Use the sphere shader
        cellShader.use();         // Set up camera matrices (only calculate once per frame, not per cell)
        glm::mat4 view = camera.getViewMatrix();

        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                                aspectRatio,
                                                0.1f, 1000.0f);
        // Set uniforms
        cellShader.setMat4("uProjection", projection);
        cellShader.setMat4("uView", view);
        cellShader.setVec3("uCameraPos", camera.getPosition());
        // Set fixed world-space directional light (like sunlight)
        cellShader.setVec3("uLightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
        // Set selection highlighting uniforms
        if (selectedCell.isValid)
        {
            glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndMass);
            float selectedRadius = selectedCell.cellData.getRadius();
            cellShader.setVec3("uSelectedCellPos", selectedPos);
            cellShader.setFloat("uSelectedCellRadius", selectedRadius);
        }
        else
        {
            cellShader.setVec3("uSelectedCellPos", glm::vec3(-9999.0f)); // Invalid position
            cellShader.setFloat("uSelectedCellRadius", 0.0f);
        }
        cellShader.setFloat("uTime", static_cast<float>(glfwGetTime())); // Enable depth testing for proper 3D rendering (don't clear here - already done in main loop)
        glEnable(GL_DEPTH_TEST);
        
        // Enable back face culling for better performance
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        
        // Set polygon mode based on wireframe flag
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            // Keep culling enabled in wireframe mode to see the effect of back face culling
            // Uncomment the next line ONLY if you want to see all triangles (front and back)
            // glDisable(GL_CULL_FACE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        // Render instanced spheres with appropriate count
        int renderCount = (useFrustumCulling || useDistanceCulling || useLODSystem) ? visibleCellCount : totalCellCount;
        sphereMesh.render(renderCount);
        
        // Restore OpenGL state - disable culling for other rendering operations
        glDisable(GL_CULL_FACE);
        // Restore polygon mode to fill
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        
        // Render sphere skin after cells
        renderSphereSkin(camera, resolution);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in renderCells: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Unknown exception in renderCells\n";
    }
}

void CellManager::runCollisionCompute()
{
    TimerGPU timer("Cell Collision Compute");

    physicsShader->use();

    // Set uniforms
    physicsShader->setFloat("u_accelerationDamping", 0.8f); // More aggressive damping

    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    physicsShader->setInt("u_draggedCellIndex", draggedIndex);
    
    // Disable thrust force for preview simulation
    physicsShader->setInt("u_enableThrustForce", isPreviewSimulation ? 0 : 1);

    // Set spatial grid uniforms
    physicsShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    physicsShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    physicsShader->setFloat("u_worldSize", config::WORLD_SIZE);
    physicsShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID); // Bind buffers (read from previous buffer, write to current buffer for stable simulation)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer()); // Read from previous frame
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridCountBuffer);

    // Also bind current buffer as output for physics results
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, getCellWriteBuffer()); // Write to current frame
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, gpuCellCountBuffer); // Bind GPU cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, modeBuffer); // Bind mode buffer for thrust force

    // Dispatch compute shader - OPTIMIZED for 256 work group size
    GLuint numGroups = (totalCellCount + 255) / 256; // Changed from 64 to 256 for better GPU utilization
    physicsShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Swap buffers for next frame
    rotateBuffers();
}

void CellManager::runAdhesionPhysics(float deltaTime)
{
    TimerGPU timer("Adhesion Physics Compute");

    adhesionPhysicsShader->use();

    // Set uniforms
    adhesionPhysicsShader->setFloat("u_deltaTime", deltaTime);

    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    adhesionPhysicsShader->setInt("u_draggedCellIndex", draggedIndex);

    // Bind buffers for adhesion physics
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());  // Read cell data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);           // Mode data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adhesionConnectionBuffer); // Adhesion connections
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);   // Cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, getCellWriteBuffer()); // Write cell data

    // Dispatch compute shader
    GLuint numGroups = (totalCellCount + 255) / 256;
    adhesionPhysicsShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Swap buffers for next frame
    rotateBuffers();
}

void CellManager::runPositionUpdateCompute(float deltaTime)
{
    TimerGPU timer("Cell Position Update Compute");

	positionUpdateShader->use();

    // Set uniforms
    positionUpdateShader->setFloat("u_deltaTime", deltaTime);

    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    positionUpdateShader->setInt("u_draggedCellIndex", draggedIndex); // Bind current cell buffer for in-place updates

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCellWriteBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer); // Bind GPU cell count buffer

    // Dispatch compute shader - OPTIMIZED for 256 work group size
    GLuint numGroups = (totalCellCount + 255) / 256; // Changed from 64 to 256 for better GPU utilization
    positionUpdateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Swap buffers for next frame
    rotateBuffers();
}

void CellManager::runVelocityUpdateCompute(float deltaTime)
{
    TimerGPU timer("Cell Velocity Update Compute");

    velocityUpdateShader->use();

    // Set uniforms
    velocityUpdateShader->setFloat("u_deltaTime", deltaTime);
    velocityUpdateShader->setFloat("u_damping", 0.98f);

    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    velocityUpdateShader->setInt("u_draggedCellIndex", draggedIndex);
    
    // Set sphere barrier uniforms
    velocityUpdateShader->setFloat("u_sphereRadius", config::SPHERE_RADIUS);
    velocityUpdateShader->setVec3("u_sphereCenter", config::SPHERE_CENTER);
    velocityUpdateShader->setInt("u_enableVelocityBarrier", config::ENABLE_VELOCITY_BARRIER ? 1 : 0);
    velocityUpdateShader->setFloat("u_barrierDamping", config::BARRIER_DAMPING);
    velocityUpdateShader->setFloat("u_barrierPushDistance", config::BARRIER_PUSH_DISTANCE);
    // Bind current cell buffer for in-place updates

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCellWriteBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer); // Bind GPU cell count buffer

    // Dispatch compute shader - OPTIMIZED for 256 work group size
    GLuint numGroups = (totalCellCount + 255) / 256; // Changed from 64 to 256 for better GPU utilization
    velocityUpdateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Swap buffers for next frame
    rotateBuffers();
}

void CellManager::runInternalUpdateCompute(float deltaTime)
{
    TimerGPU timer("Cell Internal Update Compute");

    internalUpdateShader->use();

    // Set uniforms
    internalUpdateShader->setFloat("u_deltaTime", deltaTime);
    internalUpdateShader->setInt("u_maxCells", cellLimit);
    internalUpdateShader->setInt("u_maxAdhesions", getAdhesionLimit());
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, getCellWriteBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, adhesionConnectionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, freeCellSlotBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, freeAdhesionSlotBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, uniqueIdBuffer);

    // Dispatch compute shader - OPTIMIZED for 256 work group size
    GLuint numGroups = (totalCellCount + 255) / 256; // Changed from 64 to 256 for better GPU utilization
    internalUpdateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // CRITICAL FIX: Ensure all buffers are consistent after splitting
    // Copy the write buffer to the standby buffer to maintain consistency
    glCopyNamedBufferSubData(getCellWriteBuffer(), cellBuffer[getRotatedIndex(2, 3)], 0, 0, totalCellCount * sizeof(ComputeCell));

    // Swap buffers for next frame
    rotateBuffers();
    
    // CRITICAL FIX: Add safety check for high cell counts
    if (totalCellCount > cellLimit * 0.95) { // If we're at 95% capacity
        // Force a memory barrier to ensure all operations are complete
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    }
}

void CellManager::applyCellAdditions()
{
    TimerGPU timer("Cell Additions");

    cellAdditionShader->use();

    // Set uniforms
    cellAdditionShader->setInt("u_maxCells", cellLimit);
    cellAdditionShader->setInt("u_pendingCellCount", pendingCellCount);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellAdditionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, getCellWriteBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, uniqueIdBuffer);

    // Dispatch compute shader
    GLuint numGroups = (pendingCellCount + 63) / 64;
    cellAdditionShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // CRITICAL FIX: Rotate buffers after adding cells to ensure they're in the correct buffer for next frame
    rotateBuffers();
}

// ============================================================================
// SIMULATION RESET & CLEANUP
// ============================================================================

void CellManager::resetSimulation()
{
    // Clear CPU-side data
    cpuCells.clear();
    cellStagingBuffer.clear();
    totalCellCount = 0;
    liveCellCount = 0;
    pendingCellCount = 0;
    totalAdhesionCount = 0;
    liveAdhesionCount = 0;
    
    // CRITICAL FIX: Reset buffer rotation state for consistent keyframe restoration
    bufferRotation = 0;
    
    // Clear selection state
    clearSelection();
    
    // Reset lineage tracking
    nextUniqueId = 1;
    
    // Clear GPU buffers by setting them to zero
    GLuint zero = 0;
    
    // Reset cell count buffers
    glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint), &zero); // totalCellCount = 0
    glNamedBufferSubData(gpuCellCountBuffer, sizeof(GLuint), sizeof(GLuint), &zero); // liveCellCount = 0
    glNamedBufferSubData(gpuCellCountBuffer, 2 * sizeof(GLuint), sizeof(GLuint), &zero); // totalAdhesionCount = 0
    glNamedBufferSubData(gpuCellCountBuffer, 3 * sizeof(GLuint), sizeof(GLuint), &zero); // liveAdhesionCount = 0
    
    // Clear all cell buffers
    for (int i = 0; i < 3; i++)
    {
        if (cellBuffer[i] != 0) {
            glClearNamedBufferData(cellBuffer[i], GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        }
    }

    if (instanceBuffer != 0) {
        glClearNamedBufferData(instanceBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }

    // Clear mode buffer to prevent stale mode data
    if (modeBuffer != 0) {
        glClearNamedBufferData(modeBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }

    // Clear free slot buffers
    if (freeCellSlotBuffer != 0) {
        glClearNamedBufferData(freeCellSlotBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    if (freeAdhesionSlotBuffer != 0) {
        glClearNamedBufferData(freeAdhesionSlotBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }

    // Clear addition buffer
    if (cellAdditionBuffer != 0) {
        glClearNamedBufferData(cellAdditionBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    
    // Reset unique ID buffer
    if (uniqueIdBuffer != 0) {
        GLuint initialId = 1;
        glNamedBufferSubData(uniqueIdBuffer, 0, sizeof(GLuint), &initialId);
    }
    
    // Clear spatial grid buffers
    if (gridBuffer != 0) {
        glClearNamedBufferData(gridBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    if (gridCountBuffer != 0) {
        glClearNamedBufferData(gridCountBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    if (gridOffsetBuffer != 0) {
        glClearNamedBufferData(gridOffsetBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    if (gridHashBuffer != 0) {
        glClearNamedBufferData(gridHashBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    if (activeCellsBuffer != 0) {
        glClearNamedBufferData(activeCellsBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    
    // Clear adhesionSettings line buffer to prevent lingering lines after reset
    if (adhesionLineBuffer != 0) {
        glClearNamedBufferData(adhesionLineBuffer, GL_R32F, GL_RED, GL_FLOAT, nullptr);
    }
    if (adhesionLineVBO != 0) {
        glClearNamedBufferData(adhesionLineVBO, GL_R32F, GL_RED, GL_FLOAT, nullptr);
    }
    
    // Clear adhesionSettings connection buffer to prevent lingering connections after reset
    if (adhesionConnectionBuffer != 0) {
        glClearNamedBufferData(adhesionConnectionBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    totalAdhesionCount = 0;
    
    // Clear debug visualization buffers to prevent lingering elements after reset
    if (gizmoBuffer != 0) {
        glClearNamedBufferData(gizmoBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    if (ringGizmoBuffer != 0) {
        glClearNamedBufferData(ringGizmoBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    
    // Clear LOD and frustum culling buffers
    for (int i = 0; i < 4; i++) {
        if (lodInstanceBuffers[i] != 0) {
            glClearNamedBufferData(lodInstanceBuffers[i], GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        }
        lodInstanceCounts[i] = 0; // Reset CPU-side LOD counts
    }
    
    // Invalidate cache since LOD counts have been reset
    invalidateStatisticsCache();
    if (lodCountBuffer != 0) {
        glClearNamedBufferData(lodCountBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    // Clear unified output buffers
    for (int i = 0; i < 4; i++) {
        if (unifiedOutputBuffers[i] != 0) {
            glClearNamedBufferData(unifiedOutputBuffers[i], GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        }
    }
    if (unifiedCountBuffer != 0) {
        glClearNamedBufferData(unifiedCountBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    visibleCellCount = 0; // Reset visible cell count
}

// ============================================================================
// CELL SPAWNING & RESET
// ============================================================================

void CellManager::spawnCells(int count)
{
    TimerCPU cpuTimer("Spawning Cells");

    for (int i = 0; i < count && totalCellCount < cellLimit; ++i)
    {
        // Random position within spawn radius
        float angle1 = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float angle2 = static_cast<float>(rand()) / RAND_MAX * 3.14159f;
        float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;

        glm::vec3 position = glm::vec3(
            radius * sin(angle2) * cos(angle1),
            radius * cos(angle2),
            radius * sin(angle2) * sin(angle1));

        // Random velocity
        glm::vec3 velocity = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f); // Random mass and fixed radius of 1
        //float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        //float cellRadius = 1.0f; // All cells have the same radius of 1

        ComputeCell newCell{};
        newCell.positionAndMass = glm::vec4(position, 1.);
        newCell.velocity = glm::vec4(velocity, 0.);
        newCell.acceleration = glm::vec4(0.0f); // Reset acceleration
        
        // Assign lineage ID for root cells
        newCell.parentLineageId = 0;  // Root cells have no parent
        newCell.uniqueId = nextUniqueId++;
        newCell.childNumber = 0;      // Root cells are not children

        addCellToStagingBuffer(newCell);
    }
}

void CellManager::updateCounts()
{
    syncCounterBuffers();

    // Add buffer update barrier but don't flush yet
    addBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    totalCellCount = countPtr[0];
    liveCellCount = countPtr[1];
    totalAdhesionCount = countPtr[2]; // This is the number of adhesion connections, not cells
    liveAdhesionCount = totalAdhesionCount - countPtr[3];
    
    // CRITICAL FIX: Clamp cell counts to prevent overflow
    if (totalCellCount > cellLimit) {
        totalCellCount = cellLimit;
        countPtr[0] = static_cast<GLuint>(cellLimit);
    }
    if (liveCellCount > cellLimit) {
        liveCellCount = cellLimit;
        countPtr[1] = static_cast<GLuint>(cellLimit);
    }
    
    // CRITICAL FIX: If we had to clamp values, update the GPU buffer
    if (countPtr[0] != totalCellCount || countPtr[1] != liveCellCount) {
        GLuint counts[4] = { 
            static_cast<GLuint>(totalCellCount), 
            static_cast<GLuint>(liveCellCount), 
            static_cast<GLuint>(totalAdhesionCount), 
            static_cast<GLuint>(liveAdhesionCount) 
        };
        glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint) * 4, counts);
    }
}

// ============================================================================
// GLOBAL FLAGELLOCYTE SETTINGS
// ============================================================================

void CellManager::loadGlobalFlagellocyteSettings()
{
    const std::string filename = "../../shaders/rendering/cell_types/flagellocyte/settings.txt";
    std::ifstream file(filename);
    
    if (!file.is_open()) {
        std::cout << "No saved flagellocyte settings found at " << filename << ", using defaults\n";
        // Don't try to save - just use defaults
        return;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        
        if (key == "tailLength") {
            iss >> globalFlagellocyteSettings.tailLength;
        } else if (key == "tailThickness") {
            iss >> globalFlagellocyteSettings.tailThickness;
        } else if (key == "spiralTightness") {
            iss >> globalFlagellocyteSettings.spiralTightness;
        } else if (key == "spiralRadius") {
            iss >> globalFlagellocyteSettings.spiralRadius;
        } else if (key == "rotationSpeed") {
            iss >> globalFlagellocyteSettings.rotationSpeed;
        } else if (key == "tailTaper") {
            iss >> globalFlagellocyteSettings.tailTaper;
        } else if (key == "segments") {
            iss >> globalFlagellocyteSettings.segments;
        } else if (key == "tailColor") {
            iss >> globalFlagellocyteSettings.tailColor.r 
                >> globalFlagellocyteSettings.tailColor.g 
                >> globalFlagellocyteSettings.tailColor.b;
        }
    }
    
    file.close();
    std::cout << "Loaded global flagellocyte settings from " << filename << "\n";
    std::cout << "  tailLength: " << globalFlagellocyteSettings.tailLength << "\n";
    std::cout << "  tailThickness: " << globalFlagellocyteSettings.tailThickness << "\n";
    std::cout << "  spiralTightness: " << globalFlagellocyteSettings.spiralTightness << "\n";
    std::cout << "  segments: " << globalFlagellocyteSettings.segments << "\n";
}

void CellManager::saveGlobalFlagellocyteSettings()
{
    const std::string filename = "../../shaders/rendering/cell_types/flagellocyte/settings.txt";
    std::ofstream file(filename);
    
    if (!file.is_open()) {
        std::cerr << "Failed to save global flagellocyte settings to " << filename << "\n";
        return;
    }
    
    file << "tailLength " << globalFlagellocyteSettings.tailLength << "\n";
    file << "tailThickness " << globalFlagellocyteSettings.tailThickness << "\n";
    file << "spiralTightness " << globalFlagellocyteSettings.spiralTightness << "\n";
    file << "spiralRadius " << globalFlagellocyteSettings.spiralRadius << "\n";
    file << "rotationSpeed " << globalFlagellocyteSettings.rotationSpeed << "\n";
    file << "tailTaper " << globalFlagellocyteSettings.tailTaper << "\n";
    file << "segments " << globalFlagellocyteSettings.segments << "\n";
    file << "tailColor " << globalFlagellocyteSettings.tailColor.r << " " 
         << globalFlagellocyteSettings.tailColor.g << " " 
         << globalFlagellocyteSettings.tailColor.b << "\n";
    
    file.close();
    std::cout << "Saved global flagellocyte settings to " << filename << "\n";
}