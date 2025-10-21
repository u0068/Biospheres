#pragma once

#include <vector>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../core/shader_class.h"

// Forward declarations
class SpatialGridSystem;

/**
 * VisualizationRenderer - Handles rendering of fluid visualization elements
 * 
 * This class provides rendering capabilities for:
 * - Density wireframes around voxels containing density
 * - Flow lines showing velocity field direction and magnitude
 * - Density visualization with configurable thresholds and color mapping
 */
class VisualizationRenderer {
public:
    // Visualization modes
    enum class VisualizationMode {
        None = 0,
        DensityWireframe = 1,
        FlowLines = 2,
        DensityVisualization = 4,
        All = 7  // Bitfield combination
    };
    
    // Configuration structure
    struct VisualizationConfig {
        // General settings
        int visualizationMode = static_cast<int>(VisualizationMode::None);
        bool enablePerformanceOptimization = true;
        
        // Density wireframe settings
        glm::vec4 wireframeColor = glm::vec4(0.0f, 1.0f, 0.0f, 0.5f); // Green, semi-transparent
        float densityThreshold = 0.01f;
        float maxDensity = 1.0f;
        bool enableColorMapping = true;
        
        // Flow line settings
        glm::vec4 baseLineColor = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f); // Bright magenta, fully opaque
        float minVelocityThreshold = 0.001f;
        float maxVelocity = 10.0f;
        float maxLineLength = 2.0f;
        float lineWidthScale = 1.0f;
        bool enableVelocityColoring = true;
        float alphaFade = 0.0f;
        
        // Density visualization settings
        glm::vec4 baseColor = glm::vec4(0.0f, 0.5f, 1.0f, 0.6f); // Blue, semi-transparent
        float alphaMultiplier = 1.0f;
    };

private:
    // Shader programs
    std::unique_ptr<Shader> flowLineGenerationShader;
    std::unique_ptr<Shader> densityWireframeShader;
    std::unique_ptr<Shader> densityVisualizationShader;
    std::unique_ptr<Shader> flowLineRenderShader;
    std::unique_ptr<Shader> voxelCompactionShader;
    std::unique_ptr<Shader> updateIndirectCommandsShader;
    
    // GPU resources for wireframe rendering
    GLuint wireframeVAO = 0;
    GLuint wireframeVBO = 0; // Cube vertices
    GLuint wireframeInstanceVBO = 0; // Instance data (position, density)
    GLuint wireframeEBO = 0; // Cube indices for wireframe
    
    // GPU resources for flow line rendering
    GLuint flowLineVAO = 0;
    GLuint flowLineVBO = 0; // Line vertices (0.0, 1.0)
    GLuint flowLineInstanceVBO = 0; // Instance data from compute shader
    GLuint flowLineDataBuffer = 0; // SSBO for flow line data
    
    // GPU resources for density visualization
    GLuint densityVisualizationBuffer = 0; // SSBO for visualization data
    GLuint densityPointVAO = 0;
    GLuint densityPointVBO = 0;
    
    // Optimized empty voxel skipping resources
    GLuint compactWireframeBuffer = 0; // SSBO for wireframe data only
    GLuint compactFlowLineBuffer = 0;  // SSBO for flow line data only
    GLuint voxelCountBuffer = 0;       // Atomic counter for active voxel count
    GLuint indirectDrawBuffer = 0;     // Indirect draw commands
    
    // Configuration
    VisualizationConfig config;
    
    // System state
    bool initialized = false;
    int gridResolution = 64;
    float worldSize = 100.0f;
    glm::vec3 worldCenter = glm::vec3(0.0f);
    
    // Performance tracking
    mutable size_t lastWireframeInstanceCount = 0;
    mutable size_t lastFlowLineCount = 0;
    mutable size_t lastDensityPointCount = 0;
    mutable size_t lastCompactVoxelCount = 0;

public:
    // Lifecycle
    VisualizationRenderer();
    ~VisualizationRenderer();
    
    // Non-copyable, movable
    VisualizationRenderer(const VisualizationRenderer&) = delete;
    VisualizationRenderer& operator=(const VisualizationRenderer&) = delete;
    VisualizationRenderer(VisualizationRenderer&&) noexcept;
    VisualizationRenderer& operator=(VisualizationRenderer&&) noexcept;
    
    // System management
    void initialize(int gridRes, float worldSz, const glm::vec3& worldCtr);
    void cleanup();
    bool isInitialized() const { return initialized; }
    
    // Configuration
    void setVisualizationConfig(const VisualizationConfig& newConfig);
    const VisualizationConfig& getVisualizationConfig() const { return config; }
    void setVisualizationMode(VisualizationMode mode);
    VisualizationMode getVisualizationMode() const;
    
    // Rendering methods
    void renderDensityWireframes(const SpatialGridSystem& spatialGrid, 
                                const glm::mat4& viewMatrix, 
                                const glm::mat4& projectionMatrix);
    
    void renderFlowLines(const SpatialGridSystem& spatialGrid,
                        const glm::mat4& viewMatrix,
                        const glm::mat4& projectionMatrix);
    
    void renderDensityVisualization(const SpatialGridSystem& spatialGrid,
                                   const glm::mat4& viewMatrix,
                                   const glm::mat4& projectionMatrix);
    
    // Main render function
    void render(const SpatialGridSystem& spatialGrid,
               const glm::mat4& viewMatrix,
               const glm::mat4& projectionMatrix);
    
    // Geometry generation (called automatically when needed)
    void generateWireframeGeometry(const SpatialGridSystem& spatialGrid);
    void generateFlowLineGeometry(const SpatialGridSystem& spatialGrid);
    void generateDensityVisualizationGeometry(const SpatialGridSystem& spatialGrid);
    
    // Performance and debugging
    size_t getWireframeInstanceCount() const { return lastWireframeInstanceCount; }
    size_t getFlowLineCount() const { return lastFlowLineCount; }
    size_t getDensityPointCount() const { return lastDensityPointCount; }
    size_t getCompactVoxelCount() const { return lastCompactVoxelCount; }
    size_t getTotalVoxelCount() const { return gridResolution * gridResolution * gridResolution; }
    float getVoxelSkipRatio() const { 
        size_t total = getTotalVoxelCount();
        return total > 0 ? (1.0f - (float)lastCompactVoxelCount / total) : 0.0f;
    }
    size_t getGPUMemoryUsage() const;
    void reportPerformanceStats() const;
    
    // Validation
    bool validateResources() const;

private:
    // Initialization helpers
    void initializeShaders();
    void initializeWireframeResources();
    void initializeFlowLineResources();
    void initializeDensityVisualizationResources();
    void initializeCompactVoxelResources();
    
    // Cleanup helpers
    void cleanupWireframeResources();
    void cleanupFlowLineResources();
    void cleanupDensityVisualizationResources();
    void cleanupCompactVoxelResources();
    
    // Optimized voxel processing
    void generateCompactVoxelList(const SpatialGridSystem& spatialGrid);
    void renderCompactWireframes(const SpatialGridSystem& spatialGrid,
                                const glm::mat4& viewMatrix,
                                const glm::mat4& projectionMatrix);
    void renderCompactFlowLines(const SpatialGridSystem& spatialGrid,
                               const glm::mat4& viewMatrix,
                               const glm::mat4& projectionMatrix);
    
    // Geometry helpers
    void createCubeWireframeGeometry();
    void createLineGeometry();
    void createPointGeometry();
    

    
    // Utility functions
    void setCommonUniforms(Shader& shader, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix);
    void bindSpatialGridTextures(const SpatialGridSystem& spatialGrid);
    void addWireframeLine(std::vector<float>& vertices, const glm::vec3& start, const glm::vec3& end);
};