#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

// Forward declarations
class Shader;
class MeshLibrary;
struct AdhesionConnection;

/**
 * BridgeRenderingSystem
 * 
 * Renders organic bridge connections between adhesion-connected cells using
 * pre-baked metaball mesh variations. Uses GPU-driven rendering pipeline with
 * compute shaders for instance generation and instanced rendering for bridges.
 * 
 * Requirements: 11.1, 11.2, 11.3
 */
class BridgeRenderingSystem {
public:
    BridgeRenderingSystem();
    ~BridgeRenderingSystem();

    // Core lifecycle methods (Requirement 11.1, 11.2)
    void initialize();
    void update(float deltaTime);
    void render(const glm::mat4& viewProj);
    void cleanup();

    // Integration with CellManager (Requirement 11.3, 11.4)
    void setCellData(GLuint cellDataSSBO, int cellCount);
    void setAdhesionConnections(const std::vector<AdhesionConnection>& connections);
    
    // Mesh loading (Requirement 4.4, 12.1)
    void loadBakedMeshes(const std::string& directory);

    // Configuration
    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool isEnabled() const { return m_enabled; }
    
    // Statistics (Requirement 16.1)
    int getInstanceCount() const { return m_currentInstanceCount; }
    float getLastFrameTime() const { return m_lastFrameTime; }
    float getLastComputeTime() const { return m_lastComputeTime; }
    float getLastRenderTime() const { return m_lastRenderTime; }

private:
    // Initialization helpers
    void initializeBuffers();
    void initializeShaders();
    
    // Buffer management helpers (Requirement 14.1, 14.2)
    void bindBuffersForCompute();
    void bindBuffersForRendering();
    
    // GPU buffers (Requirement 11.1, 11.2)
    GLuint m_cellDataSSBO;              // Reference to CellManager's cell data (not owned)
    GLuint m_adhesionConnectionSSBO;    // Adhesion connections from simulation
    GLuint m_bridgeInstanceSSBO;        // Generated bridge instances
    GLuint m_indirectDrawBuffer;        // Indirect draw commands
    
    // Shader programs (Requirement 11.1, 11.2)
    Shader* m_instanceGenShader;        // Compute shader for instance generation
    Shader* m_bridgeRenderShader;       // Vertex/fragment shaders for bridge rendering
    Shader* m_cellCoreShader;           // Vertex/fragment shaders for cell cores
    
    // Mesh library (Requirement 4.4, 12.1, 12.2)
    MeshLibrary* m_meshLibrary;         // Manages baked mesh variations
    
    // State tracking (Requirement 11.3, 11.4)
    int m_cellCount;                    // Current cell count from CellManager
    int m_adhesionCount;                // Current adhesion connection count
    int m_currentInstanceCount;         // Number of bridge instances generated
    
    // Configuration
    bool m_initialized;
    bool m_enabled;
    
    // Performance tracking (Requirement 16.1)
    float m_lastFrameTime;              // Last frame time in milliseconds
    GLuint m_queryBegin;                // GPU timer query for frame start
    GLuint m_queryEnd;                  // GPU timer query for frame end
    GLuint m_computeQueryBegin;         // GPU timer query for compute shader start
    GLuint m_computeQueryEnd;           // GPU timer query for compute shader end
    float m_lastComputeTime;            // Last compute shader time in milliseconds
    float m_lastRenderTime;             // Last rendering time in milliseconds
    
    // Performance monitoring helpers
    void initializePerformanceQueries();
    void beginPerformanceMeasurement();
    void endPerformanceMeasurement();
    void updatePerformanceMetrics();
    
    // Maximum capacities
    static constexpr int MAX_BRIDGE_INSTANCES = 100000;
};
