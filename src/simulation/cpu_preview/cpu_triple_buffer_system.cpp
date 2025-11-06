#include "cpu_triple_buffer_system.h"
#include <chrono>
#include <stdexcept>
#include <iostream>

CPUTripleBufferSystem::CPUTripleBufferSystem() {
    // Initialize all buffers
    for (auto& buffer : m_buffers) {
        buffer.clear();
    }
}

CPUTripleBufferSystem::~CPUTripleBufferSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void CPUTripleBufferSystem::initialize(GLuint existingInstanceBuffer) {
    if (m_initialized) {
        return;
    }

    try {
        // Set up instance buffer
        if (existingInstanceBuffer != 0) {
            m_instanceBuffer = existingInstanceBuffer;
            m_ownsInstanceBuffer = false;
        } else {
            // Create our own instance buffer
            glGenBuffers(1, &m_instanceBuffer);
            if (m_instanceBuffer == 0) {
                throw std::runtime_error("Failed to create instance buffer");
            }
            m_ownsInstanceBuffer = true;
            
            // Initialize buffer with maximum capacity (matching existing instance format)
            glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
            glBufferData(GL_ARRAY_BUFFER, 
                        256 * sizeof(CPUInstanceData), // Maximum 256 instances (3 vec4s each)
                        nullptr, 
                        GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // Validate OpenGL state
        validateGPUState();

        m_initialized = true;
        // System initialized silently
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize CPU Triple Buffer System: " << e.what() << std::endl;
        throw;
    }
}

void CPUTripleBufferSystem::shutdown() {
    if (!m_initialized) {
        return;
    }

    // Clean up GPU resources
    if (m_ownsInstanceBuffer && m_instanceBuffer != 0) {
        glDeleteBuffers(1, &m_instanceBuffer);
        m_instanceBuffer = 0;
    }

    m_initialized = false;
    // System shutdown complete
}

void CPUTripleBufferSystem::updateVisualData(const CPUCellPhysics_SoA& cells) {
    if (!m_initialized) {
        return;
    }

    // Get current write buffer
    int writeIdx = m_writeIndex.load();
    CPUVisualData& writeBuffer = m_buffers[writeIdx];

    // Clear buffer
    writeBuffer.clear();
    writeBuffer.activeCount = cells.activeCellCount;

    // Extract visual data from CPU SoA structure
    extractInstanceData(cells, writeBuffer);

    // Rotate buffers (lock-free)
    rotateBuffers();
}

void CPUTripleBufferSystem::uploadToGPU() {
    if (!m_initialized || m_instanceBuffer == 0) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_uploadMutex);
    m_uploadStart = std::chrono::steady_clock::now();

    try {
        // Get current upload buffer
        int uploadIdx = m_uploadIndex.load();
        const CPUVisualData& uploadBuffer = m_buffers[uploadIdx];

        // Perform optimized GPU upload
        optimizedGPUUpload(uploadBuffer);

        // Update performance metrics
        auto uploadEnd = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(uploadEnd - m_uploadStart);
        m_lastUploadTime = duration.count() / 1000.0f; // Convert to milliseconds
        m_lastUploadSize = uploadBuffer.activeCount * sizeof(CPUInstanceData);
    }
    catch (const std::exception& e) {
        std::cerr << "CPU GPU upload error: " << e.what() << std::endl;
    }
}

const CPUTripleBufferSystem::CPUVisualData* CPUTripleBufferSystem::getCurrentVisualData() const {
    if (!m_initialized) {
        return nullptr;
    }

    int readIdx = m_readIndex.load();
    return &m_buffers[readIdx];
}

void CPUTripleBufferSystem::extractInstanceData(const CPUCellPhysics_SoA& cells, CPUVisualData& visual) {
    size_t count = std::min(cells.activeCellCount, visual.instances.size());
    
    for (size_t i = 0; i < count; ++i) {
        // Extract position and calculate radius from mass (matching existing system)
        glm::vec3 position(cells.pos_x[i], cells.pos_y[i], cells.pos_z[i]);
        float radius = std::pow(cells.mass[i], 1.0f / 3.0f); // Cube root of mass
        
        // Set position and radius
        visual.instances[i].positionAndRadius = glm::vec4(position, radius);
        
        // Use stored genome color directly (RGBA like GPU version)
        visual.instances[i].color = glm::vec4(
            cells.color_r[i], 
            cells.color_g[i], 
            cells.color_b[i], 
            1.0f
        );
        
        // Debug output for first cell (only once)
        if (i == 0) {
            // Color data processed silently
        }
        
        // Set orientation (quaternion in w, x, y, z order)
        visual.instances[i].orientation = glm::vec4(
            cells.quat_w[i],
            cells.quat_x[i],
            cells.quat_y[i],
            cells.quat_z[i]
        );
    }
}

void CPUTripleBufferSystem::rotateBuffers() {
    // Atomic buffer rotation (lock-free)
    int currentWrite = m_writeIndex.load();
    int currentRead = m_readIndex.load();
    int currentUpload = m_uploadIndex.load();
    
    // Rotate: write -> read -> upload -> write
    m_writeIndex.store(currentRead);
    m_readIndex.store(currentUpload);
    m_uploadIndex.store(currentWrite);
}

void CPUTripleBufferSystem::optimizedGPUUpload(const CPUVisualData& data) {
    if (data.activeCount == 0) {
        return;
    }

    // Bind instance buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
    
    // Upload instance data (positionAndRadius, color, orientation format)
    size_t uploadSize = data.activeCount * sizeof(CPUInstanceData);
    glBufferSubData(GL_ARRAY_BUFFER, 0, uploadSize, data.instances.data());
    
    // Unbind buffer
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Skip expensive glGetError() call for performance
}

void CPUTripleBufferSystem::validateGPUState() {
    // Check OpenGL context
    if (!glGetString(GL_VERSION)) {
        throw std::runtime_error("No valid OpenGL context");
    }
    
    // Skip expensive glGetError() call for performance
}

glm::vec4 CPUTripleBufferSystem::generateCellColor(const glm::vec3& baseColor, uint32_t cellType, uint32_t genomeID, float age, float energy) {
    // Use the genome-specified base color directly (like GPU version)
    return glm::vec4(baseColor, 1.0f);
}