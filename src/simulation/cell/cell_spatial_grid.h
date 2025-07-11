#pragma once
#include <glad/glad.h>
#include "../../rendering/core/shader_class.h"

// Forward declarations
class CellShaderManager;

struct CellSpatialGrid
{
    // Spatial partitioning buffers - Double buffered
    GLuint gridBuffer{};       // SSBO for grid cell data (stores cell indices)
    GLuint gridCountBuffer{};  // SSBO for grid cell counts
    GLuint gridOffsetBuffer{}; // SSBO for grid cell starting offsets
    
    // PERFORMANCE OPTIMIZATION: Additional buffers for 100k cells
    GLuint gridHashBuffer{};   // Hash-based lookup for sparse grids
    GLuint activeCellsBuffer{}; // Buffer containing only active grid cells
    uint32_t activeGridCount{0}; // Number of active grid cells

    // Reference to shader manager for grid operations
    CellShaderManager* shaderManager = nullptr;

    // Constructor and destructor
    CellSpatialGrid(CellShaderManager* shaderMgr = nullptr);
    ~CellSpatialGrid();

    // Spatial grid management functions
    void initializeSpatialGrid();
    void updateSpatialGrid();
    void cleanupSpatialGrid();

    // Setter for shader manager reference
    void setShaderManager(CellShaderManager* shaderMgr) { shaderManager = shaderMgr; }
}; 