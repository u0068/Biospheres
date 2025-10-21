#include "visualization_renderer.h"
#include "../../simulation/spatial/spatial_grid_system.h"
#include "../../core/config.h"
#include <iostream>
#include <algorithm>

// Cube wireframe vertices (unit cube centered at origin)
static const float cubeVertices[] = {
    // Front face
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    // Back face
    -0.5f, -0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f
};

// Cube wireframe indices (lines)
static const unsigned int cubeIndices[] = {
    // Front face
    0, 1, 1, 2, 2, 3, 3, 0,
    // Back face
    4, 5, 5, 6, 6, 7, 7, 4,
    // Connecting edges
    0, 4, 1, 5, 2, 6, 3, 7
};

// Line vertices (for flow lines)
static const float lineVertices[] = {
    0.0f, 0.0f, 0.0f,  // Start point
    1.0f, 0.0f, 0.0f   // End point (will be scaled by direction and length)
};

VisualizationRenderer::VisualizationRenderer() = default;

VisualizationRenderer::~VisualizationRenderer() {
    cleanup();
}

VisualizationRenderer::VisualizationRenderer(VisualizationRenderer&& other) noexcept
    : flowLineGenerationShader(std::move(other.flowLineGenerationShader))
    , densityWireframeShader(std::move(other.densityWireframeShader))
    , densityVisualizationShader(std::move(other.densityVisualizationShader))
    , flowLineRenderShader(std::move(other.flowLineRenderShader))
    , wireframeVAO(other.wireframeVAO)
    , wireframeVBO(other.wireframeVBO)
    , wireframeInstanceVBO(other.wireframeInstanceVBO)
    , wireframeEBO(other.wireframeEBO)
    , flowLineVAO(other.flowLineVAO)
    , flowLineVBO(other.flowLineVBO)
    , flowLineInstanceVBO(other.flowLineInstanceVBO)
    , flowLineDataBuffer(other.flowLineDataBuffer)
    , densityVisualizationBuffer(other.densityVisualizationBuffer)
    , densityPointVAO(other.densityPointVAO)
    , densityPointVBO(other.densityPointVBO)
    , compactWireframeBuffer(other.compactWireframeBuffer)
    , compactFlowLineBuffer(other.compactFlowLineBuffer)
    , voxelCountBuffer(other.voxelCountBuffer)
    , indirectDrawBuffer(other.indirectDrawBuffer)
    , config(other.config)
    , initialized(other.initialized)
    , gridResolution(other.gridResolution)
    , worldSize(other.worldSize)
    , worldCenter(other.worldCenter) {
    
    // Reset other object
    other.wireframeVAO = 0;
    other.wireframeVBO = 0;
    other.wireframeInstanceVBO = 0;
    other.wireframeEBO = 0;
    other.flowLineVAO = 0;
    other.flowLineVBO = 0;
    other.flowLineInstanceVBO = 0;
    other.flowLineDataBuffer = 0;
    other.densityVisualizationBuffer = 0;
    other.densityPointVAO = 0;
    other.densityPointVBO = 0;
    other.compactWireframeBuffer = 0;
    other.compactFlowLineBuffer = 0;
    other.voxelCountBuffer = 0;
    other.indirectDrawBuffer = 0;
    other.initialized = false;
}

VisualizationRenderer& VisualizationRenderer::operator=(VisualizationRenderer&& other) noexcept {
    if (this != &other) {
        cleanup();
        
        flowLineGenerationShader = std::move(other.flowLineGenerationShader);
        densityWireframeShader = std::move(other.densityWireframeShader);
        densityVisualizationShader = std::move(other.densityVisualizationShader);
        flowLineRenderShader = std::move(other.flowLineRenderShader);
        wireframeVAO = other.wireframeVAO;
        wireframeVBO = other.wireframeVBO;
        wireframeInstanceVBO = other.wireframeInstanceVBO;
        wireframeEBO = other.wireframeEBO;
        flowLineVAO = other.flowLineVAO;
        flowLineVBO = other.flowLineVBO;
        flowLineInstanceVBO = other.flowLineInstanceVBO;
        flowLineDataBuffer = other.flowLineDataBuffer;
        densityVisualizationBuffer = other.densityVisualizationBuffer;
        densityPointVAO = other.densityPointVAO;
        densityPointVBO = other.densityPointVBO;
        compactWireframeBuffer = other.compactWireframeBuffer;
        compactFlowLineBuffer = other.compactFlowLineBuffer;
        voxelCountBuffer = other.voxelCountBuffer;
        indirectDrawBuffer = other.indirectDrawBuffer;
        config = other.config;
        initialized = other.initialized;
        gridResolution = other.gridResolution;
        worldSize = other.worldSize;
        worldCenter = other.worldCenter;
        
        // Reset other object
        other.wireframeVAO = 0;
        other.wireframeVBO = 0;
        other.wireframeInstanceVBO = 0;
        other.wireframeEBO = 0;
        other.flowLineVAO = 0;
        other.flowLineVBO = 0;
        other.flowLineInstanceVBO = 0;
        other.flowLineDataBuffer = 0;
        other.densityVisualizationBuffer = 0;
        other.densityPointVAO = 0;
        other.densityPointVBO = 0;
        other.compactWireframeBuffer = 0;
        other.compactFlowLineBuffer = 0;
        other.voxelCountBuffer = 0;
        other.indirectDrawBuffer = 0;
        other.initialized = false;
    }
    return *this;
}

void VisualizationRenderer::initialize(int gridRes, float worldSz, const glm::vec3& worldCtr) {
    if (initialized) {
        cleanup();
    }
    
    gridResolution = gridRes;
    worldSize = worldSz;
    worldCenter = worldCtr;
    
    try {
        initializeShaders();
        initializeWireframeResources();
        initializeFlowLineResources();
        initializeDensityVisualizationResources();
        initializeCompactVoxelResources();
        
        initialized = true;
    } catch (const std::exception& e) {
        std::cerr << "VisualizationRenderer initialization failed: " << e.what() << std::endl;
        cleanup();
        throw;
    }
}

void VisualizationRenderer::cleanup() {
    cleanupWireframeResources();
    cleanupFlowLineResources();
    cleanupDensityVisualizationResources();
    cleanupCompactVoxelResources();
    
    // Reset shaders
    flowLineGenerationShader.reset();
    densityWireframeShader.reset();
    densityVisualizationShader.reset();
    flowLineRenderShader.reset();
    voxelCompactionShader.reset();
    updateIndirectCommandsShader.reset();
    
    initialized = false;
}

void VisualizationRenderer::initializeShaders() {
    try {
        // Initialize compute shader for flow line generation
        std::cout << "Loading flow line generation compute shader..." << std::endl;
        flowLineGenerationShader = std::make_unique<Shader>("shaders/volumetric/flow_line_generation.comp");
        std::cout << "Flow line generation shader ID: " << flowLineGenerationShader->ID << std::endl;
        
        // Initialize wireframe rendering shaders
        std::cout << "Loading wireframe shaders..." << std::endl;
        densityWireframeShader = std::make_unique<Shader>("shaders/volumetric/density_wireframe.vert", 
                                                          "shaders/volumetric/density_wireframe.frag");
        std::cout << "Wireframe shader ID: " << densityWireframeShader->ID << std::endl;
        
        // Initialize density visualization compute shader
        std::cout << "Loading density visualization compute shader..." << std::endl;
        densityVisualizationShader = std::make_unique<Shader>("shaders/volumetric/density_visualization.comp");
        std::cout << "Density visualization shader ID: " << densityVisualizationShader->ID << std::endl;
        
        // Initialize flow line rendering shaders
        std::cout << "Loading flow line render shaders..." << std::endl;
        flowLineRenderShader = std::make_unique<Shader>("shaders/volumetric/flow_line_render.vert",
                                                        "shaders/volumetric/flow_line_render.frag");
        std::cout << "Flow line render shader ID: " << flowLineRenderShader->ID << std::endl;
        
        // Initialize voxel compaction compute shader
        std::cout << "Loading voxel compaction compute shader..." << std::endl;
        voxelCompactionShader = std::make_unique<Shader>("shaders/volumetric/voxel_compaction.comp");
        std::cout << "Voxel compaction shader ID: " << voxelCompactionShader->ID << std::endl;
        
        // Initialize indirect command update compute shader
        std::cout << "Loading indirect command update compute shader..." << std::endl;
        updateIndirectCommandsShader = std::make_unique<Shader>("shaders/volumetric/update_indirect_commands.comp");
        std::cout << "Indirect command update shader ID: " << updateIndirectCommandsShader->ID << std::endl;
        
        // All visualization shaders loaded
    } catch (const std::exception& e) {
        std::cerr << "Failed to load visualization shaders: " << e.what() << std::endl;
        throw;
    } catch (...) {
        std::cerr << "Unknown error loading visualization shaders" << std::endl;
        throw;
    }
}

void VisualizationRenderer::initializeWireframeResources() {
    // Generate VAO
    glGenVertexArrays(1, &wireframeVAO);
    glBindVertexArray(wireframeVAO);
    
    // Create cube vertices VBO
    glGenBuffers(1, &wireframeVBO);
    glBindBuffer(GL_ARRAY_BUFFER, wireframeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    // Set vertex attributes for cube vertices
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Create instance data VBO (will be updated per frame)
    glGenBuffers(1, &wireframeInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, wireframeInstanceVBO);
    
    // Reserve space for maximum possible instances (all voxels)
    size_t maxInstances = gridResolution * gridResolution * gridResolution;
    size_t instanceDataSize = maxInstances * (3 + 1) * sizeof(float); // position + density
    glBufferData(GL_ARRAY_BUFFER, instanceDataSize, nullptr, GL_DYNAMIC_DRAW);
    
    // Set vertex attributes for instance data
    // Instance position (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    
    // Instance density (location 2)
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    // Create element buffer for wireframe indices
    glGenBuffers(1, &wireframeEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireframeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    
    glBindVertexArray(0);
}

void VisualizationRenderer::initializeFlowLineResources() {
    // Generate VAO
    glGenVertexArrays(1, &flowLineVAO);
    glBindVertexArray(flowLineVAO);
    
    // Create line vertices VBO (simple line from 0 to 1)
    glGenBuffers(1, &flowLineVBO);
    glBindBuffer(GL_ARRAY_BUFFER, flowLineVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVertices), lineVertices, GL_STATIC_DRAW);
    
    // Set vertex attributes for line vertices (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Create instance VBO for flow line data
    glGenBuffers(1, &flowLineInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, flowLineInstanceVBO);
    
    // Reserve space for instance data (8 floats per instance)
    size_t maxFlowLines = gridResolution * gridResolution * gridResolution;
    size_t instanceDataSize = maxFlowLines * 8 * sizeof(float); // 8 floats per flow line
    glBufferData(GL_ARRAY_BUFFER, instanceDataSize, nullptr, GL_DYNAMIC_DRAW);
    
    // Set vertex attributes for instance data
    // Line start data (location 1) - [startPos.xyz, length]
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    
    // Line direction data (location 2) - [direction.xyz, magnitude]
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    // Keep flow line data buffer for compatibility
    glGenBuffers(1, &flowLineDataBuffer);
    
    glBindVertexArray(0);
}

void VisualizationRenderer::initializeDensityVisualizationResources() {
    // Generate VAO
    glGenVertexArrays(1, &densityPointVAO);
    glBindVertexArray(densityPointVAO);
    
    // Create point VBO (single point at origin)
    glGenBuffers(1, &densityPointVBO);
    glBindBuffer(GL_ARRAY_BUFFER, densityPointVBO);
    float pointVertex[] = {0.0f, 0.0f, 0.0f};
    glBufferData(GL_ARRAY_BUFFER, sizeof(pointVertex), pointVertex, GL_STATIC_DRAW);
    
    // Set vertex attributes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Create density visualization data buffer (SSBO)
    glGenBuffers(1, &densityVisualizationBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, densityVisualizationBuffer);
    
    // Reserve space for visualization data (2 vec4s per voxel)
    size_t maxPoints = gridResolution * gridResolution * gridResolution;
    size_t visualizationDataSize = maxPoints * 2 * 4 * sizeof(float); // 2 vec4s per point
    glBufferData(GL_SHADER_STORAGE_BUFFER, visualizationDataSize, nullptr, GL_DYNAMIC_DRAW);
    
    glBindVertexArray(0);
}

void VisualizationRenderer::cleanupWireframeResources() {
    if (wireframeEBO) { glDeleteBuffers(1, &wireframeEBO); wireframeEBO = 0; }
    if (wireframeInstanceVBO) { glDeleteBuffers(1, &wireframeInstanceVBO); wireframeInstanceVBO = 0; }
    if (wireframeVBO) { glDeleteBuffers(1, &wireframeVBO); wireframeVBO = 0; }
    if (wireframeVAO) { glDeleteVertexArrays(1, &wireframeVAO); wireframeVAO = 0; }
}

void VisualizationRenderer::cleanupFlowLineResources() {
    if (flowLineDataBuffer) { glDeleteBuffers(1, &flowLineDataBuffer); flowLineDataBuffer = 0; }
    if (flowLineInstanceVBO) { glDeleteBuffers(1, &flowLineInstanceVBO); flowLineInstanceVBO = 0; }
    if (flowLineVBO) { glDeleteBuffers(1, &flowLineVBO); flowLineVBO = 0; }
    if (flowLineVAO) { glDeleteVertexArrays(1, &flowLineVAO); flowLineVAO = 0; }
}

void VisualizationRenderer::cleanupDensityVisualizationResources() {
    if (densityVisualizationBuffer) { glDeleteBuffers(1, &densityVisualizationBuffer); densityVisualizationBuffer = 0; }
    if (densityPointVBO) { glDeleteBuffers(1, &densityPointVBO); densityPointVBO = 0; }
    if (densityPointVAO) { glDeleteVertexArrays(1, &densityPointVAO); densityPointVAO = 0; }
}

void VisualizationRenderer::initializeCompactVoxelResources() {
    size_t maxVoxels = gridResolution * gridResolution * gridResolution;
    
    // Create compact wireframe buffer for non-empty voxels
    glGenBuffers(1, &compactWireframeBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, compactWireframeBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxVoxels * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW); // vec4 per voxel
    
    // Create compact flow line buffer for non-empty voxels
    glGenBuffers(1, &compactFlowLineBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, compactFlowLineBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxVoxels * 8 * sizeof(float), nullptr, GL_DYNAMIC_DRAW); // 2 vec4s per voxel
    
    // Create atomic counter buffer for tracking active voxel count
    glGenBuffers(1, &voxelCountBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
    
    // Create indirect draw buffer for GPU-driven rendering
    glGenBuffers(1, &indirectDrawBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, indirectDrawBuffer);
    
    // Reserve space for two sets of 5 uint32 values (wireframe and flow line commands)
    glBufferData(GL_SHADER_STORAGE_BUFFER, 10 * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void VisualizationRenderer::cleanupCompactVoxelResources() {
    if (indirectDrawBuffer) { glDeleteBuffers(1, &indirectDrawBuffer); indirectDrawBuffer = 0; }
    if (voxelCountBuffer) { glDeleteBuffers(1, &voxelCountBuffer); voxelCountBuffer = 0; }
    if (compactFlowLineBuffer) { glDeleteBuffers(1, &compactFlowLineBuffer); compactFlowLineBuffer = 0; }
    if (compactWireframeBuffer) { glDeleteBuffers(1, &compactWireframeBuffer); compactWireframeBuffer = 0; }
}

void VisualizationRenderer::setVisualizationConfig(const VisualizationConfig& newConfig) {
    config = newConfig;
}

void VisualizationRenderer::setVisualizationMode(VisualizationMode mode) {
    config.visualizationMode = static_cast<int>(mode);
}

VisualizationRenderer::VisualizationMode VisualizationRenderer::getVisualizationMode() const {
    return static_cast<VisualizationMode>(config.visualizationMode);
}

void VisualizationRenderer::render(const SpatialGridSystem& spatialGrid,
                                  const glm::mat4& viewMatrix,
                                  const glm::mat4& projectionMatrix) {
    if (!initialized) {
        return;
    }
    
    // Enable blending for transparent visualization
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Generate compact voxel list once for all visualization modes
    generateCompactVoxelList(spatialGrid);
    
    // Render based on enabled modes using optimized compact rendering
    if ((config.visualizationMode & static_cast<int>(VisualizationMode::DensityWireframe)) != 0) {
        renderCompactWireframes(spatialGrid, viewMatrix, projectionMatrix);
    }
    
    if ((config.visualizationMode & static_cast<int>(VisualizationMode::FlowLines)) != 0) {
        renderCompactFlowLines(spatialGrid, viewMatrix, projectionMatrix);
    }
    
    if ((config.visualizationMode & static_cast<int>(VisualizationMode::DensityVisualization)) != 0) {
        renderDensityVisualization(spatialGrid, viewMatrix, projectionMatrix);
    }
    
    glDisable(GL_BLEND);
}

void VisualizationRenderer::renderDensityWireframes(const SpatialGridSystem& spatialGrid,
                                                   const glm::mat4& viewMatrix,
                                                   const glm::mat4& projectionMatrix) {
    // Generate wireframe geometry
    generateWireframeGeometry(spatialGrid);
    
    if (lastWireframeInstanceCount == 0) {
        return; // Nothing to render
    }
    
    // Use wireframe shader
    densityWireframeShader->use();
    
    // Set common uniforms
    setCommonUniforms(*densityWireframeShader, viewMatrix, projectionMatrix);
    
    // Set wireframe-specific uniforms
    densityWireframeShader->setVec4("u_wireframeColor", config.wireframeColor.x, config.wireframeColor.y, config.wireframeColor.z, config.wireframeColor.w);
    densityWireframeShader->setFloat("u_densityThreshold", config.densityThreshold);
    densityWireframeShader->setFloat("u_maxDensity", config.maxDensity);
    densityWireframeShader->setFloat("u_voxelSize", worldSize / gridResolution);
    densityWireframeShader->setInt("u_enableColorMapping", config.enableColorMapping ? 1 : 0);
    densityWireframeShader->setInt("u_enableWireframe", 1);
    
    // Render wireframes
    glBindVertexArray(wireframeVAO);
    glDrawElementsInstanced(GL_LINES, static_cast<GLsizei>(sizeof(cubeIndices) / sizeof(unsigned int)), 
                           GL_UNSIGNED_INT, 0, static_cast<GLsizei>(lastWireframeInstanceCount));
    glBindVertexArray(0);
}

void VisualizationRenderer::renderFlowLines(const SpatialGridSystem& spatialGrid,
                                           const glm::mat4& viewMatrix,
                                           const glm::mat4& projectionMatrix) {
    // Generate flow line geometry
    generateFlowLineGeometry(spatialGrid);
    
    if (lastFlowLineCount == 0) {
        std::cout << "Flow line rendering: No flow lines to render" << std::endl;
        return; // Nothing to render
    }
    
    // Rendering flow lines
    
    // Use flow line render shader
    if (!flowLineRenderShader) {
        std::cerr << "Flow line render shader is null!" << std::endl;
        return;
    }
    
    flowLineRenderShader->use();
    
    // Check for OpenGL errors after shader use
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after using flow line shader: " << error << std::endl;
        return;
    }
    
    // Set common uniforms
    setCommonUniforms(*flowLineRenderShader, viewMatrix, projectionMatrix);
    
    // Set flow line-specific uniforms
    flowLineRenderShader->setVec4("u_baseLineColor", config.baseLineColor.x, config.baseLineColor.y, config.baseLineColor.z, config.baseLineColor.w);
    flowLineRenderShader->setInt("u_enableFlowLines", 1);
    
    // Set line width for better visibility
    glLineWidth(3.0f);
    
    // Render flow lines
    glBindVertexArray(flowLineVAO);
    glDrawArraysInstanced(GL_LINES, 0, 2, static_cast<GLsizei>(lastFlowLineCount)); // 2 vertices per line
    glBindVertexArray(0);
    
    // Reset line width
    glLineWidth(1.0f);
}

void VisualizationRenderer::renderDensityVisualization(const SpatialGridSystem& spatialGrid,
                                                      const glm::mat4& viewMatrix,
                                                      const glm::mat4& projectionMatrix) {
    // Generate density visualization geometry
    generateDensityVisualizationGeometry(spatialGrid);
    
    if (lastDensityPointCount == 0) {
        return; // Nothing to render
    }
    
    // Enable point size modification in vertex shader
    glEnable(GL_PROGRAM_POINT_SIZE);
    
    // Render points (implementation would need a point rendering shader)
    // For now, this is a placeholder - would need additional point rendering shaders
    
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void VisualizationRenderer::generateWireframeGeometry(const SpatialGridSystem& spatialGrid) {
    // Early exit optimization - only generate if wireframes are enabled
    if ((config.visualizationMode & static_cast<int>(VisualizationMode::DensityWireframe)) == 0) {
        lastWireframeInstanceCount = 0;
        return;
    }
    
    std::vector<float> instanceData;
    lastWireframeInstanceCount = 0;
    
    // Optimized iteration with early termination
    int maxInstances = 1000; // Limit to prevent FPS drops
    
    for (int x = 0; x < gridResolution && lastWireframeInstanceCount < maxInstances; ++x) {
        for (int y = 0; y < gridResolution && lastWireframeInstanceCount < maxInstances; ++y) {
            for (int z = 0; z < gridResolution && lastWireframeInstanceCount < maxInstances; ++z) {
                glm::ivec3 gridPos(x, y, z);
                float density = spatialGrid.getDensity(gridPos);
                
                if (density > config.densityThreshold) {
                    glm::vec3 worldPos = spatialGrid.gridToWorld(gridPos);
                    
                    // Add instance data: position (3 floats) + density (1 float)
                    instanceData.push_back(worldPos.x);
                    instanceData.push_back(worldPos.y);
                    instanceData.push_back(worldPos.z);
                    instanceData.push_back(density);
                    
                    lastWireframeInstanceCount++;
                }
            }
        }
    }
    
    // Upload instance data to GPU
    if (!instanceData.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, wireframeInstanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(float), instanceData.data());
    }
}

void VisualizationRenderer::generateFlowLineGeometry(const SpatialGridSystem& spatialGrid) {
    // Early exit optimization - only generate if flow lines are enabled
    if ((config.visualizationMode & static_cast<int>(VisualizationMode::FlowLines)) == 0) {
        lastFlowLineCount = 0;
        return;
    }
    
    std::vector<float> instanceData;
    lastFlowLineCount = 0;
    
    // Optimized iteration with early termination
    int maxInstances = 1000; // Limit to prevent FPS drops
    
    for (int x = 0; x < gridResolution && lastFlowLineCount < maxInstances; ++x) {
        for (int y = 0; y < gridResolution && lastFlowLineCount < maxInstances; ++y) {
            for (int z = 0; z < gridResolution && lastFlowLineCount < maxInstances; ++z) {
                glm::ivec3 gridPos(x, y, z);
                glm::vec3 velocity = spatialGrid.getVelocity(gridPos);
                float velocityMagnitude = glm::length(velocity);
                
                if (velocityMagnitude >= config.minVelocityThreshold) {
                    glm::vec3 worldPos = spatialGrid.gridToWorld(gridPos);
                    glm::vec3 direction = glm::normalize(velocity);
                    float lineLength = std::min(velocityMagnitude * config.maxLineLength, config.maxLineLength);
                    
                    // Instance data: [startPos.xyz, length, direction.xyz, magnitude]
                    instanceData.push_back(worldPos.x);
                    instanceData.push_back(worldPos.y);
                    instanceData.push_back(worldPos.z);
                    instanceData.push_back(lineLength);
                    instanceData.push_back(direction.x);
                    instanceData.push_back(direction.y);
                    instanceData.push_back(direction.z);
                    instanceData.push_back(velocityMagnitude);
                    
                    lastFlowLineCount++;
                }
            }
        }
    }
    
    // Upload instance data to GPU
    if (!instanceData.empty()) {
        glBindBuffer(GL_ARRAY_BUFFER, flowLineInstanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(float), instanceData.data());
    }
}

void VisualizationRenderer::generateDensityVisualizationGeometry(const SpatialGridSystem& spatialGrid) {
    if (!densityVisualizationShader) {
        return;
    }
    
    // Use compute shader to generate density visualization data
    densityVisualizationShader->use();
    
    // Bind density texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, spatialGrid.getDensityTexture());
    densityVisualizationShader->setInt("u_densityTexture", 0);
    
    // Bind visualization data buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, densityVisualizationBuffer);
    
    // Set uniforms
    densityVisualizationShader->setInt("u_gridResolution", gridResolution);
    densityVisualizationShader->setFloat("u_worldSize", worldSize);
    densityVisualizationShader->setVec3("u_worldCenter", worldCenter);
    densityVisualizationShader->setFloat("u_densityThreshold", config.densityThreshold);
    densityVisualizationShader->setFloat("u_maxDensity", config.maxDensity);
    densityVisualizationShader->setVec4("u_baseColor", config.baseColor.x, config.baseColor.y, config.baseColor.z, config.baseColor.w);
    densityVisualizationShader->setInt("u_enableColorMapping", config.enableColorMapping ? 1 : 0);
    densityVisualizationShader->setInt("u_enableVisualization", 1);
    densityVisualizationShader->setFloat("u_alphaMultiplier", config.alphaMultiplier);
    
    // Dispatch compute shader
    int workGroupSize = 8;
    int numGroups = (gridResolution + workGroupSize - 1) / workGroupSize;
    densityVisualizationShader->dispatch(numGroups, numGroups, numGroups);
    
    // Wait for compute shader to complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Count actual valid density points by checking density threshold
    lastDensityPointCount = 0;
    for (int x = 0; x < gridResolution; ++x) {
        for (int y = 0; y < gridResolution; ++y) {
            for (int z = 0; z < gridResolution; ++z) {
                glm::ivec3 gridPos(x, y, z);
                float density = spatialGrid.getDensity(gridPos);
                
                if (density >= config.densityThreshold) {
                    lastDensityPointCount++;
                }
            }
        }
    }
}



void VisualizationRenderer::setCommonUniforms(Shader& shader, const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix) {
    glm::mat4 modelMatrix = glm::mat4(1.0f); // Identity matrix
    shader.setMat4("u_modelMatrix", modelMatrix);
    shader.setMat4("u_viewMatrix", viewMatrix);
    shader.setMat4("u_projectionMatrix", projectionMatrix);
}

void VisualizationRenderer::bindSpatialGridTextures(const SpatialGridSystem& spatialGrid) {
    // Bind density texture to texture unit 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, spatialGrid.getDensityTexture());
    
    // Bind velocity texture to texture unit 1
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, spatialGrid.getVelocityTexture());
}

size_t VisualizationRenderer::getGPUMemoryUsage() const {
    if (!initialized) {
        return 0;
    }
    
    size_t totalMemory = 0;
    
    // Wireframe resources
    size_t maxInstances = gridResolution * gridResolution * gridResolution;
    totalMemory += sizeof(cubeVertices); // Cube vertices
    totalMemory += sizeof(cubeIndices);  // Cube indices
    totalMemory += maxInstances * 4 * sizeof(float); // Instance data
    
    // Flow line resources
    totalMemory += sizeof(lineVertices); // Line vertices
    totalMemory += maxInstances * 2 * 4 * sizeof(float); // Flow line data buffer
    totalMemory += maxInstances * 2 * 4 * sizeof(float); // Flow line instance VBO
    
    // Compact voxel resources
    totalMemory += maxInstances * 4 * sizeof(float); // Compact wireframe buffer
    totalMemory += maxInstances * 8 * sizeof(float); // Compact flow line buffer
    totalMemory += sizeof(GLuint); // Voxel count buffer
    totalMemory += 10 * sizeof(GLuint); // Indirect draw buffer
    
    // Density visualization resources
    totalMemory += 3 * sizeof(float); // Point vertex
    totalMemory += maxInstances * 2 * 4 * sizeof(float); // Visualization data buffer
    
    return totalMemory;
}

void VisualizationRenderer::reportPerformanceStats() const {
    std::cout << "=== VisualizationRenderer Performance Stats ===" << std::endl;
    std::cout << "Total voxels: " << (gridResolution * gridResolution * gridResolution) << std::endl;
    std::cout << "Compact voxels: " << lastCompactVoxelCount << std::endl;
    std::cout << "Voxel skip ratio: " << (100.0f * (1.0f - (float)lastCompactVoxelCount / (gridResolution * gridResolution * gridResolution))) << "%" << std::endl;
    std::cout << "Wireframe instances: " << lastWireframeInstanceCount << std::endl;
    std::cout << "Flow lines: " << lastFlowLineCount << std::endl;
    std::cout << "Density points: " << lastDensityPointCount << std::endl;
    std::cout << "GPU memory usage: " << (getGPUMemoryUsage() / (1024 * 1024)) << " MB" << std::endl;
}

void VisualizationRenderer::addWireframeLine(std::vector<float>& vertices, const glm::vec3& start, const glm::vec3& end) {
    // Add start point
    vertices.push_back(start.x);
    vertices.push_back(start.y);
    vertices.push_back(start.z);
    
    // Add end point
    vertices.push_back(end.x);
    vertices.push_back(end.y);
    vertices.push_back(end.z);
}

void VisualizationRenderer::generateCompactVoxelList(const SpatialGridSystem& spatialGrid) {
    if (!voxelCompactionShader) {
        return;
    }
    
    // Reset atomic counter to 0
    GLuint zero = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
    
    // Use voxel compaction compute shader
    voxelCompactionShader->use();
    
    // Bind input textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, spatialGrid.getDensityTexture());
    voxelCompactionShader->setInt("u_densityTexture", 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, spatialGrid.getVelocityTexture());
    voxelCompactionShader->setInt("u_velocityTexture", 1);
    
    // Bind output buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, compactWireframeBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, voxelCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indirectDrawBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, compactFlowLineBuffer);
    
    // Set uniforms
    voxelCompactionShader->setInt("u_gridResolution", gridResolution);
    voxelCompactionShader->setFloat("u_worldSize", worldSize);
    voxelCompactionShader->setVec3("u_worldCenter", worldCenter);
    voxelCompactionShader->setFloat("u_densityThreshold", config.densityThreshold);
    voxelCompactionShader->setFloat("u_minVelocityThreshold", config.minVelocityThreshold);
    
    // Dispatch compute shader
    int workGroupSize = 8;
    int numGroups = (gridResolution + workGroupSize - 1) / workGroupSize;
    voxelCompactionShader->dispatch(numGroups, numGroups, numGroups);
    
    // Wait for compute shader to complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
    
    // Update indirect draw commands with final voxel count
    if (updateIndirectCommandsShader) {
        updateIndirectCommandsShader->use();
        
        // Bind atomic counter and indirect draw buffer
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, voxelCountBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, indirectDrawBuffer);
        
        // Dispatch single work group to update commands
        updateIndirectCommandsShader->dispatch(1, 1, 1);
        
        // Wait for command update to complete
        glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    }
    
    // Read back the compact voxel count for performance tracking
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
    GLuint* count = (GLuint*)glMapBuffer(GL_ATOMIC_COUNTER_BUFFER, GL_READ_ONLY);
    if (count) {
        lastCompactVoxelCount = *count;
        glUnmapBuffer(GL_ATOMIC_COUNTER_BUFFER);
    }
}

void VisualizationRenderer::renderCompactWireframes(const SpatialGridSystem& spatialGrid,
                                                   const glm::mat4& viewMatrix,
                                                   const glm::mat4& projectionMatrix) {
    if (lastCompactVoxelCount == 0) {
        return; // Nothing to render
    }
    
    // Use wireframe shader
    densityWireframeShader->use();
    
    // Set common uniforms
    setCommonUniforms(*densityWireframeShader, viewMatrix, projectionMatrix);
    
    // Set wireframe-specific uniforms
    densityWireframeShader->setVec4("u_wireframeColor", config.wireframeColor.x, config.wireframeColor.y, config.wireframeColor.z, config.wireframeColor.w);
    densityWireframeShader->setFloat("u_densityThreshold", config.densityThreshold);
    densityWireframeShader->setFloat("u_maxDensity", config.maxDensity);
    densityWireframeShader->setFloat("u_voxelSize", worldSize / gridResolution);
    densityWireframeShader->setInt("u_enableColorMapping", config.enableColorMapping ? 1 : 0);
    densityWireframeShader->setInt("u_enableWireframe", 1);
    
    // Bind compact wireframe data as instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, compactWireframeBuffer);
    
    // Update vertex attributes to use compact data
    glBindVertexArray(wireframeVAO);
    
    // Instance position (location 1) - from wireframe data
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    
    // Instance density (location 2) - from wireframe data
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    // Render using indirect draw (GPU-driven instance count)
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer);
    glDrawElementsIndirect(GL_LINES, GL_UNSIGNED_INT, (void*)0);
    
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    
    lastWireframeInstanceCount = lastCompactVoxelCount;
}

void VisualizationRenderer::renderCompactFlowLines(const SpatialGridSystem& spatialGrid,
                                                   const glm::mat4& viewMatrix,
                                                   const glm::mat4& projectionMatrix) {
    if (lastCompactVoxelCount == 0) {
        return; // Nothing to render
    }
    
    // Use flow line render shader
    if (!flowLineRenderShader) {
        std::cerr << "Flow line render shader is null!" << std::endl;
        return;
    }
    
    flowLineRenderShader->use();
    
    // Set common uniforms
    setCommonUniforms(*flowLineRenderShader, viewMatrix, projectionMatrix);
    
    // Set flow line-specific uniforms
    flowLineRenderShader->setVec4("u_baseLineColor", config.baseLineColor.x, config.baseLineColor.y, config.baseLineColor.z, config.baseLineColor.w);
    flowLineRenderShader->setInt("u_enableFlowLines", 1);
    
    // Bind compact flow line data as instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, compactFlowLineBuffer);
    
    // Update vertex attributes to use compact data
    glBindVertexArray(flowLineVAO);
    
    // Line start data (location 1) - [startPos.xyz, length] - every 2nd vec4
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribDivisor(1, 1);
    
    // Line direction data (location 2) - [direction.xyz, magnitude] - every 2nd vec4 + 1
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1);
    
    // Set line width for better visibility
    glLineWidth(3.0f);
    
    // Render using indirect draw (GPU-driven instance count)
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer);
    glDrawArraysIndirect(GL_LINES, (void*)(sizeof(GLuint) * 5)); // Offset to second command
    
    glBindVertexArray(0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    
    // Reset line width
    glLineWidth(1.0f);
    
    lastFlowLineCount = lastCompactVoxelCount;
}

bool VisualizationRenderer::validateResources() const {
    if (!initialized) {
        return false;
    }
    
    // Check that all required resources are created
    bool valid = true;
    
    if (wireframeVAO == 0 || wireframeVBO == 0 || wireframeEBO == 0 || wireframeInstanceVBO == 0) {
        std::cerr << "Wireframe resources not properly initialized" << std::endl;
        valid = false;
    }
    
    if (flowLineVAO == 0 || flowLineVBO == 0 || flowLineInstanceVBO == 0 || flowLineDataBuffer == 0) {
        std::cerr << "Flow line resources not properly initialized" << std::endl;
        valid = false;
    }
    
    if (densityPointVAO == 0 || densityPointVBO == 0 || densityVisualizationBuffer == 0) {
        std::cerr << "Density visualization resources not properly initialized" << std::endl;
        valid = false;
    }
    
    if (compactWireframeBuffer == 0 || compactFlowLineBuffer == 0 || voxelCountBuffer == 0 || indirectDrawBuffer == 0) {
        std::cerr << "Compact voxel resources not properly initialized" << std::endl;
        valid = false;
    }
    
    if (!flowLineGenerationShader || !densityWireframeShader || 
        !densityVisualizationShader || !flowLineRenderShader || 
        !voxelCompactionShader || !updateIndirectCommandsShader) {
        std::cerr << "Shaders not properly initialized" << std::endl;
        valid = false;
    }
    
    return valid;
}