#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>
#include "cpu_soa_data_manager.h"

/**
 * CPU Triple Buffer System for Asynchronous Visual Data Management
 * 
 * Manages asynchronous visual data upload to prevent CPU-GPU synchronization stalls.
 * Extracts minimal visual subset from CPU SoA data for efficient GPU upload.
 * Coordinates with shared rendering pipeline to display data from CPU Preview System.
 * 
 * Requirements addressed: 5.1, 5.2, 5.5
 */
class CPUTripleBufferSystem {
public:
    // Visual data structure (minimal subset for rendering)
    struct CPUVisualData {
        alignas(16) std::array<glm::vec3, 256> positions;
        alignas(16) std::array<glm::quat, 256> orientations;
        alignas(16) std::array<glm::vec4, 256> colors;
        alignas(16) std::array<glm::mat4, 256> instanceMatrices;
        size_t activeCount = 0;
        
        // Clear all data
        void clear() {
            positions.fill(glm::vec3(0.0f));
            orientations.fill(glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
            colors.fill(glm::vec4(1.0f));
            instanceMatrices.fill(glm::mat4(1.0f));
            activeCount = 0;
        }
    };

    CPUTripleBufferSystem();
    ~CPUTripleBufferSystem();

    // System lifecycle
    void initialize(GLuint existingInstanceBuffer = 0);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Visual data management
    void updateVisualData(const CPUCellPhysics_SoA& cells);
    void uploadToGPU(); // Non-blocking upload
    const CPUVisualData* getCurrentVisualData() const;

    // GPU integration
    void setInstanceBuffer(GLuint buffer) { m_instanceBuffer = buffer; }
    GLuint getInstanceBuffer() const { return m_instanceBuffer; }

    // Performance monitoring
    float getLastUploadTime() const { return m_lastUploadTime; }
    size_t getLastUploadSize() const { return m_lastUploadSize; }

    // System boundaries - integrates with existing rendering pipeline
    bool isCompatibleWithExistingRenderer() const { return true; }
    bool requiresShaderChanges() const { return false; }

private:
    // Triple buffer storage
    std::array<CPUVisualData, 3> m_buffers;
    std::atomic<int> m_writeIndex{0};
    std::atomic<int> m_readIndex{1};
    std::atomic<int> m_uploadIndex{2};

    // GPU resources
    GLuint m_instanceBuffer = 0;
    bool m_ownsInstanceBuffer = false;

    // Synchronization
    std::mutex m_uploadMutex;

    // System state
    bool m_initialized = false;

    // Performance tracking
    float m_lastUploadTime = 0.0f;
    size_t m_lastUploadSize = 0;
    std::chrono::steady_clock::time_point m_uploadStart;

    // Visual data extraction methods
    void extractPositions(const CPUCellPhysics_SoA& cells, CPUVisualData& visual);
    void extractOrientations(const CPUCellPhysics_SoA& cells, CPUVisualData& visual);
    void extractColors(const CPUCellPhysics_SoA& cells, CPUVisualData& visual);
    void generateInstanceMatrices(CPUVisualData& visual);

    // Buffer rotation (lock-free)
    void rotateBuffers();
    int getNextIndex(int current) const { return (current + 1) % 3; }

    // GPU upload optimization
    void optimizedGPUUpload(const CPUVisualData& data);
    void validateGPUState();

    // Color generation based on cell properties
    glm::vec4 generateCellColor(uint32_t cellType, uint32_t genomeID, float age, float energy);
};