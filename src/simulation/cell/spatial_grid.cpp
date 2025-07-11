#include "cell_manager.h"
#include "../../core/config.h"
#include <iostream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define GLM_ENABLE_EXPERIMENTAL
#include "../../utils/timer.h"

// Spatial partitioning
void CellManager::initializeSpatialGrid()
{
    // Create double buffered grid buffers to store cell indices

    glCreateBuffers(1, &gridBuffer);
    glNamedBufferData(gridBuffer,
        config::TOTAL_GRID_CELLS * config::MAX_CELLS_PER_GRID * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create double buffered grid count buffers to store number of cells per grid cell
    glCreateBuffers(1, &gridCountBuffer);
    glNamedBufferData(gridCountBuffer,
        config::TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create double buffered grid offset buffers for prefix sum calculations
    glCreateBuffers(1, &gridOffsetBuffer);
    glNamedBufferData(gridOffsetBuffer,
        config::TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create hash buffer for sparse grid optimization
    glCreateBuffers(1, &gridHashBuffer);
    glNamedBufferData(gridHashBuffer,
        config::TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create active cells buffer for performance optimization
    glCreateBuffers(1, &activeCellsBuffer);
    glNamedBufferData(activeCellsBuffer,
        config::TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    std::cout << "Initialized double buffered spatial grid with " << config::TOTAL_GRID_CELLS
        << " grid cells (" << config::GRID_RESOLUTION << "^3)\n";
    std::cout << "Grid cell size: " << config::GRID_CELL_SIZE << "\n";
    std::cout << "Max cells per grid: " << config::MAX_CELLS_PER_GRID << "\n";
}

void CellManager::updateSpatialGrid()
{
    if (cellCount == 0)
        return;
    TimerGPU timer("Spatial Grid Update");

    // ============= PERFORMANCE OPTIMIZATIONS FOR 100K CELLS =============
    // 1. Increased grid resolution from 32³ to 64³ (262,144 grid cells)
    // 2. Reduced max cells per grid from 64 to 32 for better memory access
    // 3. Implemented proper parallel prefix sum with shared memory
    // 4. Optimized work group sizes from 64 to 256 for better GPU utilization
    // 5. Reduced memory barriers and improved dispatch efficiency
    // 6. Added early termination in physics neighbor search
    // ====================================================================

    // HIGHLY OPTIMIZED: Combined operations with minimal barriers
    // Step 1: Clear grid counts and assign cells in parallel
    runGridClear();
    runGridAssign();

    // Single barrier after clear and assign - these operations can overlap
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();

    // Step 2: Calculate prefix sum for offsets (proper implementation now)
    runGridPrefixSum();

    // Step 3: Insert cells into grid (depends on prefix sum results)
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();

    runGridInsert();

    // Add final barrier but don't flush - let caller decide when to flush
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::cleanupSpatialGrid()
{
    // Clean up double buffered spatial grid buffers

    if (gridBuffer != 0)
    {
        glDeleteBuffers(1, &gridBuffer);
        gridBuffer = 0;
    }
    if (gridCountBuffer != 0)
    {
        glDeleteBuffers(1, &gridCountBuffer);
        gridCountBuffer = 0;
    }
    if (gridOffsetBuffer != 0)
    {
        glDeleteBuffers(1, &gridOffsetBuffer);
        gridOffsetBuffer = 0;
    }
    if (gridHashBuffer != 0)
    {
        glDeleteBuffers(1, &gridHashBuffer);
        gridHashBuffer = 0;
    }
    if (activeCellsBuffer != 0)
    {
        glDeleteBuffers(1, &activeCellsBuffer);
        activeCellsBuffer = 0;
    }
}

void CellManager::runGridClear()
{
    gridClearShader->use();

    gridClearShader->setInt("u_totalGridCells", config::TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridCountBuffer);

    // OPTIMIZED: Use larger work groups for better GPU utilization
    GLuint numGroups = (config::TOTAL_GRID_CELLS + 255) / 256; // Changed from 64 to 256
    gridClearShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridAssign()
{
    gridAssignShader->use();

    gridAssignShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    gridAssignShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    gridAssignShader->setFloat("u_worldSize", config::WORLD_SIZE);

    // Use previous buffer for spatial grid to match physics compute input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer); // Bind GPU cell count buffer

    // OPTIMIZED: Use larger work groups for better memory coalescing
    GLuint numGroups = (cellCount + 255) / 256; // Changed from 64 to 256
    gridAssignShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridPrefixSum()
{
    gridPrefixSumShader->use();

    gridPrefixSumShader->setInt("u_totalGridCells", config::TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridOffsetBuffer);

    // OPTIMIZED: Use 256-sized work groups to match shader implementation
    GLuint numGroups = (config::TOTAL_GRID_CELLS + 255) / 256; // Changed from 64 to 256
    gridPrefixSumShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridInsert()
{
    gridInsertShader->use();    gridInsertShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    gridInsertShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    gridInsertShader->setFloat("u_worldSize", config::WORLD_SIZE);
    gridInsertShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID); // Use previous buffer for spatial grid to match physics compute input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridOffsetBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, gpuCellCountBuffer); // Bind GPU cell count buffer

    // OPTIMIZED: Use larger work groups for better memory coalescing
    GLuint numGroups = (cellCount + 255) / 256; // Changed from 64 to 256
    gridInsertShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}