#include "cell_spatial_grid.h"
#include "../../core/config.h"
#include <iostream>

CellSpatialGrid::CellSpatialGrid(CellShaderManager* shaderMgr)
    : shaderManager(shaderMgr)
{
    initializeSpatialGrid();
}

CellSpatialGrid::~CellSpatialGrid()
{
    cleanupSpatialGrid();
}

void CellSpatialGrid::initializeSpatialGrid()
{
    // Create spatial partitioning buffers
    glCreateBuffers(1, &gridBuffer);
    glNamedBufferData(gridBuffer,
        config::GRID_SIZE * config::GRID_SIZE * config::GRID_SIZE * config::MAX_CELLS_PER_GRID_CELL * sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    glCreateBuffers(1, &gridCountBuffer);
    glNamedBufferData(gridCountBuffer,
        config::GRID_SIZE * config::GRID_SIZE * config::GRID_SIZE * sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    glCreateBuffers(1, &gridOffsetBuffer);
    glNamedBufferData(gridOffsetBuffer,
        config::GRID_SIZE * config::GRID_SIZE * config::GRID_SIZE * sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Performance optimization buffers for 100k cells
    glCreateBuffers(1, &gridHashBuffer);
    glNamedBufferData(gridHashBuffer,
        config::MAX_CELLS * sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    glCreateBuffers(1, &activeCellsBuffer);
    glNamedBufferData(activeCellsBuffer,
        config::MAX_CELLS * sizeof(GLuint),
        nullptr,
        GL_DYNAMIC_COPY
    );

    activeGridCount = 0;
}

void CellSpatialGrid::updateSpatialGrid()
{
    if (!shaderManager) return;

    // Update spatial grid using compute shaders
    // This follows the buffer access rules - read from current read buffer, write to write buffer
    
    // Step 1: Clear grid counts
    shaderManager->runGridClear();
    
    // Step 2: Assign cells to grid
    shaderManager->runGridAssign();
    
    // Step 3: Calculate grid offsets (prefix sum)
    shaderManager->runGridPrefixSum();
    
    // Step 4: Insert cells into grid
    shaderManager->runGridInsert();
}

void CellSpatialGrid::cleanupSpatialGrid()
{
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

    activeGridCount = 0;
} 