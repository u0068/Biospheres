#include "bridge_rendering_system.h"
#include "mesh_library.h"
#include "../core/shader_class.h"
#include "../../simulation/cell/common_structs.h"
#include "../../core/config.h"
#include <iostream>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

BridgeRenderingSystem::BridgeRenderingSystem()
    : m_cellDataSSBO(0)
    , m_adhesionConnectionSSBO(0)
    , m_bridgeInstanceSSBO(0)
    , m_indirectDrawBuffer(0)
    , m_instanceGenShader(nullptr)
    , m_bridgeRenderShader(nullptr)
    , m_cellCoreShader(nullptr)
    , m_meshLibrary(nullptr)
    , m_cellCount(0)
    , m_adhesionCount(0)
    , m_currentInstanceCount(0)
    , m_initialized(false)
    , m_enabled(true)
    , m_lastFrameTime(0.0f)
{
}

BridgeRenderingSystem::~BridgeRenderingSystem()
{
    cleanup();
}

// ============================================================================
// INITIALIZATION (Requirement 11.1, 11.2)
// ============================================================================

void BridgeRenderingSystem::initialize()
{
    if (m_initialized) {
        std::cout << "BridgeRenderingSystem: Already initialized\n";
        return;
    }

    std::cout << "BridgeRenderingSystem: Initializing...\n";

    // Initialize GPU buffers
    initializeBuffers();

    // Initialize shaders
    initializeShaders();
    
    // Initialize mesh library
    m_meshLibrary = new MeshLibrary();

    m_initialized = true;
    std::cout << "BridgeRenderingSystem: Initialization complete\n";
}

void BridgeRenderingSystem::initializeBuffers()
{
    // Create adhesion connection SSBO
    // This will be updated each frame with current adhesion connections
    glCreateBuffers(1, &m_adhesionConnectionSSBO);
    glNamedBufferData(
        m_adhesionConnectionSSBO,
        MAX_BRIDGE_INSTANCES * sizeof(AdhesionConnection),
        nullptr,
        GL_DYNAMIC_COPY  // Updated by CPU, read by GPU compute shader
    );

    // Create bridge instance SSBO
    // This will be written by compute shader with generated bridge instances
    glCreateBuffers(1, &m_bridgeInstanceSSBO);
    glNamedBufferData(
        m_bridgeInstanceSSBO,
        MAX_BRIDGE_INSTANCES * 64,  // 64 bytes per instance (see design doc)
        nullptr,
        GL_DYNAMIC_COPY  // Written by GPU compute, read by GPU rendering
    );

    // Create indirect draw command buffer
    // This will be written by compute shader with instance counts
    glCreateBuffers(1, &m_indirectDrawBuffer);
    glNamedBufferData(
        m_indirectDrawBuffer,
        sizeof(GLuint) * 5,  // DrawElementsIndirectCommand structure
        nullptr,
        GL_DYNAMIC_COPY  // Written by GPU compute, read by GPU rendering
    );

    std::cout << "BridgeRenderingSystem: GPU buffers initialized\n";
}

void BridgeRenderingSystem::initializeShaders()
{
    // TODO: Create shader files and initialize shader programs
    // For now, we'll leave these as nullptr and implement in future tasks
    
    // m_instanceGenShader = new Shader("shaders/bridges/bridge_instance_gen.comp");
    // m_bridgeRenderShader = new Shader("shaders/bridges/bridge_render.vert", 
    //                                   "shaders/bridges/bridge_render.frag");
    // m_cellCoreShader = new Shader("shaders/bridges/cell_core.vert",
    //                               "shaders/bridges/cell_core.frag");

    std::cout << "BridgeRenderingSystem: Shader initialization placeholder (shaders not yet implemented)\n";
}

// ============================================================================
// CLEANUP
// ============================================================================

void BridgeRenderingSystem::cleanup()
{
    if (!m_initialized) {
        return;
    }

    std::cout << "BridgeRenderingSystem: Cleaning up...\n";

    // Delete GPU buffers
    if (m_adhesionConnectionSSBO != 0) {
        glDeleteBuffers(1, &m_adhesionConnectionSSBO);
        m_adhesionConnectionSSBO = 0;
    }
    if (m_bridgeInstanceSSBO != 0) {
        glDeleteBuffers(1, &m_bridgeInstanceSSBO);
        m_bridgeInstanceSSBO = 0;
    }
    if (m_indirectDrawBuffer != 0) {
        glDeleteBuffers(1, &m_indirectDrawBuffer);
        m_indirectDrawBuffer = 0;
    }

    // Delete shaders
    if (m_instanceGenShader) {
        m_instanceGenShader->destroy();
        delete m_instanceGenShader;
        m_instanceGenShader = nullptr;
    }
    if (m_bridgeRenderShader) {
        m_bridgeRenderShader->destroy();
        delete m_bridgeRenderShader;
        m_bridgeRenderShader = nullptr;
    }
    if (m_cellCoreShader) {
        m_cellCoreShader->destroy();
        delete m_cellCoreShader;
        m_cellCoreShader = nullptr;
    }
    
    // Delete mesh library
    if (m_meshLibrary) {
        delete m_meshLibrary;
        m_meshLibrary = nullptr;
    }

    m_initialized = false;
    std::cout << "BridgeRenderingSystem: Cleanup complete\n";
}

// ============================================================================
// INTEGRATION WITH CELLMANAGER (Requirement 11.3, 11.4)
// ============================================================================

void BridgeRenderingSystem::setCellData(GLuint cellDataSSBO, int cellCount)
{
    // Store reference to CellManager's cell data SSBO
    // We don't own this buffer, just reference it
    m_cellDataSSBO = cellDataSSBO;
    m_cellCount = cellCount;
}

void BridgeRenderingSystem::setAdhesionConnections(const std::vector<AdhesionConnection>& connections)
{
    if (!m_initialized) {
        return;
    }

    // Update adhesion connection count
    m_adhesionCount = static_cast<int>(connections.size());

    // Clamp to maximum capacity
    if (m_adhesionCount > MAX_BRIDGE_INSTANCES) {
        std::cout << "BridgeRenderingSystem: Warning - Adhesion count (" << m_adhesionCount 
                  << ") exceeds maximum (" << MAX_BRIDGE_INSTANCES << "), clamping\n";
        m_adhesionCount = MAX_BRIDGE_INSTANCES;
    }

    // Upload adhesion connections to GPU
    if (m_adhesionCount > 0) {
        glNamedBufferSubData(
            m_adhesionConnectionSSBO,
            0,
            m_adhesionCount * sizeof(AdhesionConnection),
            connections.data()
        );
    }
}

// ============================================================================
// UPDATE & RENDER (Requirement 11.1, 11.2)
// ============================================================================

void BridgeRenderingSystem::update(float deltaTime)
{
    if (!m_initialized || !m_enabled) {
        return;
    }

    // TODO: Implement bridge instance generation compute shader dispatch
    // This will be implemented in task 10 (Implement bridge instance generation compute shader)
    
    // For now, just track that we're ready to generate instances
    m_currentInstanceCount = 0;  // Will be set by compute shader in future task
}

void BridgeRenderingSystem::render(const glm::mat4& viewProj)
{
    if (!m_initialized || !m_enabled) {
        return;
    }

    // TODO: Implement bridge rendering
    // This will be implemented in task 11 (Implement bridge rendering shaders)
    
    // For now, this is a placeholder
    m_lastFrameTime = 0.0f;
}

// ============================================================================
// MESH LOADING (Requirement 4.4, 12.1)
// ============================================================================

void BridgeRenderingSystem::loadBakedMeshes(const std::string& directory)
{
    if (!m_initialized || !m_meshLibrary) {
        std::cerr << "BridgeRenderingSystem: Cannot load meshes - system not initialized\n";
        return;
    }
    
    std::cout << "BridgeRenderingSystem: Loading baked meshes from " << directory << std::endl;
    
    // Clear existing meshes
    m_meshLibrary->clear();
    
    // Load all meshes from directory
    m_meshLibrary->loadMeshDirectory(directory);
    
    std::cout << "BridgeRenderingSystem: Loaded " << m_meshLibrary->getMeshCount() 
              << " mesh variations" << std::endl;
    
    // Print unique ratio values for debugging
    const auto& sizeRatios = m_meshLibrary->getUniqueSizeRatios();
    const auto& distanceRatios = m_meshLibrary->getUniqueDistanceRatios();
    
    std::cout << "  Size ratios: ";
    for (float ratio : sizeRatios) {
        std::cout << ratio << " ";
    }
    std::cout << std::endl;
    
    std::cout << "  Distance ratios: ";
    for (float ratio : distanceRatios) {
        std::cout << ratio << " ";
    }
    std::cout << std::endl;
}
