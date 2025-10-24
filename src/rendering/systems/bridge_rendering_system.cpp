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
    , m_queryBegin(0)
    , m_queryEnd(0)
    , m_computeQueryBegin(0)
    , m_computeQueryEnd(0)
    , m_lastComputeTime(0.0f)
    , m_lastRenderTime(0.0f)
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

    // Initialize performance queries (Requirement 16.1)
    initializePerformanceQueries();

    m_initialized = true;
    std::cout << "BridgeRenderingSystem: Initialization complete\n";
}

void BridgeRenderingSystem::initializeBuffers()
{
    // Create adhesion connection SSBO (Requirement 14.1)
    // This will be updated each frame with current adhesion connections
    glCreateBuffers(1, &m_adhesionConnectionSSBO);
    glNamedBufferData(
        m_adhesionConnectionSSBO,
        MAX_BRIDGE_INSTANCES * sizeof(AdhesionConnection),
        nullptr,
        GL_DYNAMIC_COPY  // Updated by CPU, read by GPU compute shader
    );

    // Create bridge instance SSBO (Requirement 14.2)
    // This will be written by compute shader with generated bridge instances
    // Structure: [instanceCount, maxInstances, padding, padding, ...instance data...]
    glCreateBuffers(1, &m_bridgeInstanceSSBO);
    
    // Calculate total buffer size: counter data + instance data
    size_t counterSize = sizeof(GLuint) * 4;  // instanceCount, maxInstances, 2x padding
    size_t instanceDataSize = MAX_BRIDGE_INSTANCES * 64;  // 64 bytes per instance
    size_t totalSize = counterSize + instanceDataSize;
    
    glNamedBufferData(
        m_bridgeInstanceSSBO,
        totalSize,
        nullptr,
        GL_DYNAMIC_COPY  // Written by GPU compute, read by GPU rendering
    );
    
    // Initialize counter buffer with maxInstances value
    struct CounterData {
        GLuint instanceCount;
        GLuint maxInstances;
        GLuint padding1;
        GLuint padding2;
    } counterInit = { 0, static_cast<GLuint>(MAX_BRIDGE_INSTANCES), 0, 0 };
    
    glNamedBufferSubData(m_bridgeInstanceSSBO, 0, sizeof(CounterData), &counterInit);

    // Create indirect draw command buffer (Requirement 15.1)
    // This will be written by compute shader with instance counts
    glCreateBuffers(1, &m_indirectDrawBuffer);
    glNamedBufferData(
        m_indirectDrawBuffer,
        sizeof(GLuint) * 5,  // DrawElementsIndirectCommand structure
        nullptr,
        GL_DYNAMIC_COPY  // Written by GPU compute, read by GPU rendering
    );

    std::cout << "BridgeRenderingSystem: GPU buffers initialized (max instances: " 
              << MAX_BRIDGE_INSTANCES << ")\n";
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

    // Delete performance queries
    if (m_queryBegin != 0) {
        glDeleteQueries(1, &m_queryBegin);
        m_queryBegin = 0;
    }
    if (m_queryEnd != 0) {
        glDeleteQueries(1, &m_queryEnd);
        m_queryEnd = 0;
    }
    if (m_computeQueryBegin != 0) {
        glDeleteQueries(1, &m_computeQueryBegin);
        m_computeQueryBegin = 0;
    }
    if (m_computeQueryEnd != 0) {
        glDeleteQueries(1, &m_computeQueryEnd);
        m_computeQueryEnd = 0;
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
// BUFFER MANAGEMENT HELPERS (Requirement 14.1, 14.2)
// ============================================================================

void BridgeRenderingSystem::bindBuffersForCompute()
{
    // Bind buffers to shader storage buffer binding points for compute shader
    // Binding points match those defined in bridge_instance_gen.comp
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_cellDataSSBO);           // Cell data (readonly)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_adhesionConnectionSSBO); // Adhesion connections (readonly)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_bridgeInstanceSSBO);     // Bridge instances (writeonly)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_bridgeInstanceSSBO);     // Counter buffer (read-write, same buffer)
}

void BridgeRenderingSystem::bindBuffersForRendering()
{
    // Bind buffers to shader storage buffer binding points for rendering shaders
    // Binding points match those defined in bridge_render.vert
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_cellDataSSBO);       // Cell data for vertex shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_bridgeInstanceSSBO); // Bridge instances for vertex shader
    
    // Bind indirect draw buffer
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_indirectDrawBuffer);
}

// ============================================================================
// UPDATE & RENDER (Requirement 11.1, 11.2)
// ============================================================================

void BridgeRenderingSystem::update(float deltaTime)
{
    if (!m_initialized || !m_enabled) {
        m_lastFrameTime = 0.0f;
        m_lastComputeTime = 0.0f;
        m_lastRenderTime = 0.0f;
        return;
    }

    // Skip if no adhesion connections
    if (m_adhesionCount == 0) {
        m_currentInstanceCount = 0;
        m_lastFrameTime = 0.0f;
        m_lastComputeTime = 0.0f;
        m_lastRenderTime = 0.0f;
        return;
    }

    // Begin performance measurement (Requirement 16.1)
    beginPerformanceMeasurement();

    // Start compute shader timing
    glQueryCounter(m_computeQueryBegin, GL_TIMESTAMP);

    // Reset instance counter to 0 before compute shader dispatch (Requirement 14.2)
    GLuint zero = 0;
    glNamedBufferSubData(m_bridgeInstanceSSBO, 0, sizeof(GLuint), &zero);

    // Bind buffers for compute shader (Requirement 14.1, 14.2)
    bindBuffersForCompute();

    // TODO: Dispatch compute shader for bridge instance generation
    // This will be implemented in task 10 (Implement bridge instance generation compute shader)
    // Example:
    // if (m_instanceGenShader) {
    //     m_instanceGenShader->use();
    //     m_instanceGenShader->setInt("u_numAdhesions", m_adhesionCount);
    //     m_instanceGenShader->setInt("u_enableBridgeGeneration", 1);
    //     glDispatchCompute((m_adhesionCount + 31) / 32, 1, 1);
    // }

    // End compute shader timing
    glQueryCounter(m_computeQueryEnd, GL_TIMESTAMP);

    // Memory barrier to ensure compute shader writes are visible to rendering (Requirement 14.2)
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);

    // TODO: Read back instance count from GPU (will be set by compute shader)
    // For now, assume all adhesions generate instances
    m_currentInstanceCount = m_adhesionCount;
}

void BridgeRenderingSystem::render(const glm::mat4& viewProj)
{
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Skip rendering if no instances
    if (m_currentInstanceCount == 0) {
        m_lastFrameTime = 0.0f;
        return;
    }

    // Bind buffers for rendering (Requirement 15.1)
    bindBuffersForRendering();

    // TODO: Implement bridge rendering with shaders
    // This will be implemented in task 11 (Implement bridge rendering shaders)
    // Example:
    // if (m_bridgeRenderShader && m_meshLibrary) {
    //     m_bridgeRenderShader->use();
    //     m_bridgeRenderShader->setMat4("u_viewProj", viewProj);
    //     
    //     // Render bridges using instanced rendering
    //     // The mesh library will provide the base mesh geometry
    //     // The instance buffer provides per-instance transformation data
    //     glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, nullptr);
    // }

    // Memory barrier after rendering (if needed for subsequent operations)
    // Not strictly necessary here since we're not reading back the data immediately
    // glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // End performance measurement (Requirement 16.1)
    endPerformanceMeasurement();

    // Update performance metrics (non-blocking)
    updatePerformanceMetrics();
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

// ============================================================================
// PERFORMANCE MONITORING (Requirement 16.1)
// ============================================================================

void BridgeRenderingSystem::initializePerformanceQueries()
{
    // Create GPU timer queries for performance monitoring
    glGenQueries(1, &m_queryBegin);
    glGenQueries(1, &m_queryEnd);
    glGenQueries(1, &m_computeQueryBegin);
    glGenQueries(1, &m_computeQueryEnd);
    
    std::cout << "BridgeRenderingSystem: Performance queries initialized\n";
}

void BridgeRenderingSystem::beginPerformanceMeasurement()
{
    // Record timestamp at the beginning of the frame
    glQueryCounter(m_queryBegin, GL_TIMESTAMP);
}

void BridgeRenderingSystem::endPerformanceMeasurement()
{
    // Record timestamp at the end of the frame
    glQueryCounter(m_queryEnd, GL_TIMESTAMP);
}

void BridgeRenderingSystem::updatePerformanceMetrics()
{
    // Check if query results are available (non-blocking)
    GLint available = 0;
    glGetQueryObjectiv(m_queryEnd, GL_QUERY_RESULT_AVAILABLE, &available);
    
    if (available)
    {
        // Get timestamps
        GLuint64 timeBegin = 0;
        GLuint64 timeEnd = 0;
        glGetQueryObjectui64v(m_queryBegin, GL_QUERY_RESULT, &timeBegin);
        glGetQueryObjectui64v(m_queryEnd, GL_QUERY_RESULT, &timeEnd);
        
        // Calculate frame time in milliseconds
        GLuint64 timeElapsed = timeEnd - timeBegin;
        m_lastFrameTime = static_cast<float>(timeElapsed) / 1000000.0f; // Convert nanoseconds to milliseconds
    }
    
    // Check compute shader timing if available
    GLint computeAvailable = 0;
    glGetQueryObjectiv(m_computeQueryEnd, GL_QUERY_RESULT_AVAILABLE, &computeAvailable);
    
    if (computeAvailable)
    {
        GLuint64 computeBegin = 0;
        GLuint64 computeEnd = 0;
        glGetQueryObjectui64v(m_computeQueryBegin, GL_QUERY_RESULT, &computeBegin);
        glGetQueryObjectui64v(m_computeQueryEnd, GL_QUERY_RESULT, &computeEnd);
        
        GLuint64 computeElapsed = computeEnd - computeBegin;
        m_lastComputeTime = static_cast<float>(computeElapsed) / 1000000.0f;
    }
    
    // Calculate render time as difference between total and compute time
    m_lastRenderTime = m_lastFrameTime - m_lastComputeTime;
}
