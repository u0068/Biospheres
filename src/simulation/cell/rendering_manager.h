#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../rendering/core/shader_class.h"
#include "../../rendering/core/mesh/sphere_mesh.h"
#include "../../rendering/systems/frustum_culling.h"

// Forward declaration
class Camera;

struct CellRenderingManager
{
    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // LOD system
    Shader* lodVertexShader = nullptr;        // Vertex shader for LOD rendering
    Shader* lodComputeShader = nullptr;       // Compute shader for LOD assignment
    GLuint lodInstanceBuffers[4]{};           // Instance buffers for each LOD level
    GLuint lodCountBuffer{};                  // Buffer to track instance counts per LOD level
    int lodInstanceCounts[4]{};               // CPU-side copy of LOD instance counts
    float lodDistances[4] = {50.0f, 100.0f, 200.0f, 400.0f}; // Distance thresholds for LOD levels
    bool useLODSystem = true;                 // Enable/disable LOD system
    
    // Unified culling system
    Shader* unifiedCullShader = nullptr;      // Unified compute shader for all culling modes
    Shader* distanceFadeShader = nullptr;     // Vertex/fragment shaders for distance-based fading
    GLuint unifiedOutputBuffers[4]{};         // Output buffers for each LOD level
    GLuint unifiedCountBuffer{};              // Buffer for LOD counts
    bool useFrustumCulling = true;            // Enable/disable frustum culling
    bool useDistanceCulling = true;           // Enable/disable distance-based culling
    Frustum currentFrustum;                   // Current camera frustum
    int visibleCellCount{0};                  // Number of visible cells after culling
    float maxRenderDistance = 1000.0f;        // Maximum distance to render cells
    float fadeStartDistance = 500.0f;         // Distance where fading begins
    float fadeEndDistance = 1000.0f;          // Distance where fading ends
    
    // Distance culling parameters
    glm::vec3 fogColor = glm::vec3(0.5f, 0.7f, 1.0f); // Atmospheric/fog color for distant cells

    // Constructor and destructor
    CellRenderingManager();
    ~CellRenderingManager();

    // LOD system
    void initializeLODSystem();
    void cleanupLODSystem();
    void updateLODLevels(const Camera& camera);
    void renderCellsLOD(glm::vec2 resolution, const Camera& camera, bool wireframe = false);
    void runLODCompute(const Camera& camera);

    // Unified culling system
    void initializeUnifiedCulling();
    void cleanupUnifiedCulling();
    void updateFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane);
    void runUnifiedCulling(const Camera& camera);
    void renderCellsUnified(glm::vec2 resolution, const Camera& camera, bool wireframe = false);

    // Distance culling
    void setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd);
    void setFogColor(const glm::vec3& color) { fogColor = color; }

    // Statistics
    int getTotalTriangleCount() const;
    int getTotalVertexCount() const;
    int getVisibleCellCount() const { return visibleCellCount; }
    float getMaxRenderDistance() const { return maxRenderDistance; }
    float getFadeStartDistance() const { return fadeStartDistance; }
    float getFadeEndDistance() const { return fadeEndDistance; }

    // Cached statistics for performance (updated when LOD counts change)
    mutable int cachedTriangleCount = -1;     // -1 means invalid cache
    mutable int cachedVertexCount = -1;       // -1 means invalid cache
    
    // Helper to invalidate cache when needed
    void invalidateStatisticsCache() const {
        cachedTriangleCount = -1;
        cachedVertexCount = -1;
    }
}; 