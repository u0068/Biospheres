#include "cell_shader_manager.h"
#include "../../core/config.h"
#include <iostream>

CellShaderManager::CellShaderManager()
{
    initializeShaders();
}

CellShaderManager::~CellShaderManager()
{
    cleanup();
}

void CellShaderManager::initializeShaders()
{
    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell/physics/cell_physics_spatial.comp"); // Use spatial partitioning version
    updateShader = new Shader("shaders/cell/physics/cell_update.comp");
    internalUpdateShader = new Shader("shaders/cell/physics/cell_update_internal.comp");
    extractShader = new Shader("shaders/cell/management/extract_instances.comp");
    cellAdditionShader = new Shader("shaders/cell/management/apply_additions.comp");

    // Initialize spatial grid shaders
    gridClearShader = new Shader("shaders/spatial/grid_clear.comp");
    gridAssignShader = new Shader("shaders/spatial/grid_assign.comp");
    gridPrefixSumShader = new Shader("shaders/spatial/grid_prefix_sum.comp");
    gridInsertShader = new Shader("shaders/spatial/grid_insert.comp");
}

void CellShaderManager::cleanup()
{
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
    if (updateShader)
    {
        updateShader->destroy();
        delete updateShader;
        updateShader = nullptr;
    }
    if (internalUpdateShader)
    {
        internalUpdateShader->destroy();
        delete internalUpdateShader;
        internalUpdateShader = nullptr;
    }
    if (cellAdditionShader)
    {
        cellAdditionShader->destroy();
        delete cellAdditionShader;
        cellAdditionShader = nullptr;
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
}

void CellShaderManager::runPhysicsCompute(float deltaTime)
{
    if (!physicsShader) return;

    physicsShader->use();
    physicsShader->setFloat("deltaTime", deltaTime);
    
    // Dispatch compute shader
    int numGroups = (config::MAX_CELLS + 255) / 256; // 256 threads per group
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure physics computation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellShaderManager::runUpdateCompute(float deltaTime)
{
    if (!updateShader) return;

    updateShader->use();
    updateShader->setFloat("deltaTime", deltaTime);
    
    // Dispatch compute shader
    int numGroups = (config::MAX_CELLS + 255) / 256; // 256 threads per group
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure update computation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellShaderManager::runInternalUpdateCompute(float deltaTime)
{
    if (!internalUpdateShader) return;

    internalUpdateShader->use();
    internalUpdateShader->setFloat("deltaTime", deltaTime);
    
    // Dispatch compute shader
    int numGroups = (config::MAX_CELLS + 255) / 256; // 256 threads per group
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure internal update computation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellShaderManager::runGridClear()
{
    if (!gridClearShader) return;

    gridClearShader->use();
    
    // Dispatch compute shader to clear grid
    int numGroups = (config::GRID_SIZE * config::GRID_SIZE * config::GRID_SIZE + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure grid clear is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellShaderManager::runGridAssign()
{
    if (!gridAssignShader) return;

    gridAssignShader->use();
    
    // Dispatch compute shader to assign cells to grid
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure grid assignment is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellShaderManager::runGridPrefixSum()
{
    if (!gridPrefixSumShader) return;

    gridPrefixSumShader->use();
    
    // Dispatch compute shader for prefix sum
    int numGroups = (config::GRID_SIZE * config::GRID_SIZE * config::GRID_SIZE + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure prefix sum is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellShaderManager::runGridInsert()
{
    if (!gridInsertShader) return;

    gridInsertShader->use();
    
    // Dispatch compute shader to insert cells into grid
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure grid insertion is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
} 