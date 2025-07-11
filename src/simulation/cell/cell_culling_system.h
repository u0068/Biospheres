#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../rendering/core/shader_class.h"
#include "../../rendering/systems/frustum_culling.h"

// Forward declarations
class Camera;

struct CellCullingSystem
{
    // Unified culling system
    Shader* unifiedCullShader = nullptr;      // Unified compute shader for all culling modes
    Shader* distanceFadeShader = nullptr;     // Vertex/fragment shaders for distance-based fading
    GLuint unifiedOutputBuffers[4]{};         // Output buffers for each LOD level
    GLuint unifiedCountBuffer{};              // Buffer for LOD counts
    bool useFrustumCulling = config::defaultUseFrustumCulling;            // Enable/disable frustum culling
    bool useDistanceCulling = config::defaultUseDistanceCulling;          // Enable/disable distance-based culling
    Frustum currentFrustum;                   // Current camera frustum
    int visibleCellCount{0};                  // Number of visible cells after culling
    float maxRenderDistance = config::defaultMaxRenderDistance;         // Maximum distance to render cells
    float fadeStartDistance = config::defaultFadeStartDistance;          // Distance where fading begins
    float fadeEndDistance = config::defaultFadeEndDistance;              // Distance where fading ends
    
    // Distance culling parameters
    glm::vec3 fogColor = config::defaultFogColor; // Atmospheric/fog color for distant cells

    // Constructor and destructor
    CellCullingSystem();
    ~CellCullingSystem();

    // Culling system management functions
    void initializeUnifiedCulling();
    void cleanupUnifiedCulling();
    void updateFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane);
    void runUnifiedCulling(const Camera& camera);
    void renderCellsUnified(glm::vec2 resolution, const Camera& camera, bool wireframe = false);
    void setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd);

    // Getters
    int getVisibleCellCount() const { return visibleCellCount; }
    float getMaxRenderDistance() const { return maxRenderDistance; }
    float getFadeStartDistance() const { return fadeStartDistance; }
    float getFadeEndDistance() const { return fadeEndDistance; }
    void setFogColor(const glm::vec3& color) { fogColor = color; }
}; 