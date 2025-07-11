#pragma once
#include <glad/glad.h>
#include "../../rendering/core/shader_class.h"

// Forward declarations
class Camera;

struct CellLODSystem
{
    // LOD system
    Shader* lodVertexShader = nullptr;        // Vertex shader for LOD rendering
    Shader* lodComputeShader = nullptr;       // Compute shader for LOD assignment
    GLuint lodInstanceBuffers[4]{};           // Instance buffers for each LOD level
    GLuint lodCountBuffer{};                  // Buffer to track instance counts per LOD level
    int lodInstanceCounts[4]{};               // CPU-side copy of LOD instance counts
    float lodDistances[4] = {
        config::defaultLodDistance0,
        config::defaultLodDistance1,
        config::defaultLodDistance2,
        config::defaultLodDistance3
    }; // Distance thresholds for LOD levels
    bool useLODSystem = config::defaultUseLodSystem;          // Enable/disable LOD system

    // LOD statistics functions
    int getTotalTriangleCount() const;        // Calculate total triangles across all LOD levels
    int getTotalVertexCount() const;          // Calculate total vertices across all LOD levels
    
    // Cached statistics for performance (updated when LOD counts change)
    mutable int cachedTriangleCount = -1;     // -1 means invalid cache
    mutable int cachedVertexCount = -1;       // -1 means invalid cache
    
    // Helper to invalidate cache when needed
    void invalidateStatisticsCache() const {
        cachedTriangleCount = -1;
        cachedVertexCount = -1;
    }

    // Constructor and destructor
    CellLODSystem();
    ~CellLODSystem();

    // LOD system management functions
    void initializeLODSystem();
    void cleanupLODSystem();
    void updateLODLevels(const Camera& camera);
    void renderCellsLOD(glm::vec2 resolution, const Camera& camera, bool wireframe = false);
    void runLODCompute(const Camera& camera);
}; 