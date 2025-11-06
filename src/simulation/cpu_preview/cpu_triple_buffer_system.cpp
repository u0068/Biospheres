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
            
            // Initialize buffer with maximum capacity
            glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
            glBufferData(GL_ARRAY_BUFFER, 
                        256 * sizeof(glm::mat4), // Maximum 256 instance matrices
                        nullptr, 
                        GL_DYNAMIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        // Validate OpenGL state
        validateGPUState();

        m_initialized = true;
        std::cout << "CPU Triple Buffer System initialized successfully\n";
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
    std::cout << "CPU Triple Buffer System shutdown complete\n";
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
    extractPositions(cells, writeBuffer);
    extractOrientations(cells, writeBuffer);
    extractColors(cells, writeBuffer);
    generateInstanceMatrices(writeBuffer);

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
        m_lastUploadSize = uploadBuffer.activeCount * sizeof(glm::mat4);
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

void CPUTripleBufferSystem::extractPositions(const CPUCellPhysics_SoA& cells, CPUVisualData& visual) {
    size_t count = std::min(cells.activeCellCount, visual.positions.size());
    
    for (size_t i = 0; i < count; ++i) {
        visual.positions[i] = glm::vec3(
            cells.pos_x[i],
            cells.pos_y[i],
            cells.pos_z[i]
        );
    }
}

void CPUTripleBufferSystem::extractOrientations(const CPUCellPhysics_SoA& cells, CPUVisualData& visual) {
    size_t count = std::min(cells.activeCellCount, visual.orientations.size());
    
    for (size_t i = 0; i < count; ++i) {
        visual.orientations[i] = glm::quat(
            cells.quat_w[i],
            cells.quat_x[i],
            cells.quat_y[i],
            cells.quat_z[i]
        );
    }
}

void CPUTripleBufferSystem::extractColors(const CPUCellPhysics_SoA& cells, CPUVisualData& visual) {
    size_t count = std::min(cells.activeCellCount, visual.colors.size());
    
    for (size_t i = 0; i < count; ++i) {
        visual.colors[i] = generateCellColor(
            cells.cellType[i],
            cells.genomeID[i],
            cells.age[i],
            cells.energy[i]
        );
    }
}

void CPUTripleBufferSystem::generateInstanceMatrices(CPUVisualData& visual) {
    size_t count = visual.activeCount;
    
    for (size_t i = 0; i < count; ++i) {
        // Create transformation matrix from position, orientation, and scale
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), visual.positions[i]);
        glm::mat4 rotation = glm::mat4_cast(visual.orientations[i]);
        glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f)); // Uniform scale for now
        
        visual.instanceMatrices[i] = translation * rotation * scale;
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
    
    // Upload instance matrices (most efficient format for rendering)
    size_t uploadSize = data.activeCount * sizeof(glm::mat4);
    glBufferSubData(GL_ARRAY_BUFFER, 0, uploadSize, data.instanceMatrices.data());
    
    // Unbind buffer
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        throw std::runtime_error("OpenGL error during CPU GPU upload: " + std::to_string(error));
    }
}

void CPUTripleBufferSystem::validateGPUState() {
    // Check OpenGL context
    if (!glGetString(GL_VERSION)) {
        throw std::runtime_error("No valid OpenGL context");
    }
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        throw std::runtime_error("OpenGL error during validation: " + std::to_string(error));
    }
}

glm::vec4 CPUTripleBufferSystem::generateCellColor(uint32_t cellType, uint32_t genomeID, float age, float energy) {
    // Generate color based on cell properties
    glm::vec4 color(1.0f);
    
    // Base color from cell type
    switch (cellType % 4) {
        case 0: color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); break; // Red
        case 1: color = glm::vec4(0.2f, 1.0f, 0.2f, 1.0f); break; // Green
        case 2: color = glm::vec4(0.2f, 0.2f, 1.0f, 1.0f); break; // Blue
        case 3: color = glm::vec4(1.0f, 1.0f, 0.2f, 1.0f); break; // Yellow
    }
    
    // Modulate by genome ID
    float genomeHue = (genomeID % 100) / 100.0f;
    color.r = color.r * (0.5f + 0.5f * genomeHue);
    color.g = color.g * (0.5f + 0.5f * (1.0f - genomeHue));
    
    // Modulate by energy level
    float energyFactor = std::max(0.2f, std::min(1.0f, energy));
    color.r *= energyFactor;
    color.g *= energyFactor;
    color.b *= energyFactor;
    
    // Age-based fading (older cells become more transparent)
    if (age > 10.0f) {
        float ageFactor = std::max(0.3f, 1.0f - (age - 10.0f) / 50.0f);
        color.a *= ageFactor;
    }
    
    return color;
}