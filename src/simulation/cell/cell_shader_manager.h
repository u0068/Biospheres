#pragma once
#include <glad/glad.h>
#include "../../rendering/core/shader_class.h"

struct CellShaderManager
{
    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    Shader* extractShader = nullptr; // For extracting instance data efficiently
    Shader* internalUpdateShader = nullptr;
    Shader* cellAdditionShader = nullptr;

    // Spatial partitioning compute shaders
    Shader* gridClearShader = nullptr;     // Clear grid counts
    Shader* gridAssignShader = nullptr;    // Assign cells to grid
    Shader* gridPrefixSumShader = nullptr; // Calculate grid offsets
    Shader* gridInsertShader = nullptr;    // Insert cells into grid

    // Constructor and destructor
    CellShaderManager();
    ~CellShaderManager();

    // Shader management functions
    void initializeShaders();
    void cleanup();

    // Compute shader execution functions
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
    void runInternalUpdateCompute(float deltaTime);
    void runGridClear();
    void runGridAssign();
    void runGridPrefixSum();
    void runGridInsert();
}; 