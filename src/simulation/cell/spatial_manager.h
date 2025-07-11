#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../rendering/core/shader_class.h"

struct CellSpatialManager
{
    // Spatial partitioning buffers - Double buffered
    GLuint gridBuffer{};       // SSBO for grid cell data (stores cell indices)
    GLuint gridCountBuffer{};  // SSBO for grid cell counts
    GLuint gridOffsetBuffer{}; // SSBO for grid cell starting offsets
    
    // PERFORMANCE OPTIMIZATION: Additional buffers for 100k cells
    GLuint gridHashBuffer{};   // Hash-based lookup for sparse grids
    GLuint activeCellsBuffer{}; // Buffer containing only active grid cells
    uint32_t activeGridCount{0}; // Number of active grid cells

    // Spatial partitioning compute shaders
    Shader* gridClearShader = nullptr;     // Clear grid counts
    Shader* gridAssignShader = nullptr;    // Assign cells to grid
    Shader* gridPrefixSumShader = nullptr; // Calculate grid offsets
    Shader* gridInsertShader = nullptr;    // Insert cells into grid

    // Constructor and destructor
    CellSpatialManager();
    ~CellSpatialManager();

    // Spatial grid management
    void initializeSpatialGrid();
    void updateSpatialGrid();
    void cleanupSpatialGrid();

    // Grid computation
    void runGridClear();
    void runGridAssign();
    void runGridPrefixSum();
    void runGridInsert();
}; 