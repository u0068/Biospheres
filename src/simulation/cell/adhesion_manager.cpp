#include "cell_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../core/config.h"
#include "../../ui/ui_manager.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../../utils/timer.h"

void CellManager::initializeAdhesionLineBuffers()
{
    int adhesionLimit = gpuMainMaxCapacity * config::MAX_ADHESIONS_PER_CELL / 2;
    std::cout << "Initializing adhesion line buffers with capacity for " << adhesionLimit << " connections\n";
    
    // Create buffer for adhesion line vertices (each connection has 2 segments = 4 vertices)
    // Each vertex has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &adhesionLineBuffer);
    glNamedBufferData(adhesionLineBuffer,
        adhesionLimit * sizeof(AdhesionLineVertex) * 4, // 4 vertices per connection (2 segments)
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for adhesion line rendering
    glCreateVertexArrays(1, &adhesionLineVAO);
    
    // Create VBO that will be bound to the adhesion line buffer
    glCreateBuffers(1, &adhesionLineVBO);
    glNamedBufferData(adhesionLineVBO,
        adhesionLimit * sizeof(AdhesionLineVertex) * 4, // 4 vertices per connection (2 segments)
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(adhesionLineVAO, 0, adhesionLineVBO, 0, sizeof(AdhesionLineVertex));
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(adhesionLineVAO, 0);
    glVertexArrayAttribFormat(adhesionLineVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(adhesionLineVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(adhesionLineVAO, 1);
    glVertexArrayAttribFormat(adhesionLineVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(adhesionLineVAO, 1, 0);
    
    std::cout << "Adhesion line buffers initialized successfully\n";
    std::cout << "  - adhesionLineBuffer: " << adhesionLineBuffer << "\n";
    std::cout << "  - adhesionLineVAO: " << adhesionLineVAO << "\n";
    std::cout << "  - adhesionLineVBO: " << adhesionLineVBO << "\n";
}

void CellManager::updateAdhesionLineData()
{
    if (totalAdhesionCount == 0) {
        std::cout << "updateAdhesionLineData: No adhesion connections to process\n";
        return;
    }

    //std::cout << "updateAdhesionLineData: Processing " << totalAdhesionCount << " adhesion connections\n";
    //std::cout << "  - totalCellCount: " << totalCellCount << "\n";
    //std::cout << "  - liveCellCount: " << liveCellCount << "\n";
    //std::cout << "  - totalAdhesionCount: " << totalAdhesionCount << "\n";
    //std::cout << "  - liveAdhesionCount: " << liveAdhesionCount << "\n";

    TimerGPU timer("Adhesion Data Update");

    adhesionLineExtractShader->use();

    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind adhesion connection buffer as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, adhesionConnectionBuffer);
    // Bind adhesion line buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adhesionLineBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);

    // Dispatch compute shader
    GLuint numGroups = (totalAdhesionCount + 63) / 64;
    //std::cout << "updateAdhesionLineData: Dispatching " << numGroups << " compute shader groups\n";
    adhesionLineExtractShader->dispatch(numGroups, 1, 1);

    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();

    // Copy data from compute buffer to VBO for rendering
    // Each connection now has 4 vertices (2 segments)
    glCopyNamedBufferSubData(adhesionLineBuffer, adhesionLineVBO, 0, 0, 
        totalAdhesionCount * sizeof(AdhesionLineVertex) * 4);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    //std::cout << "updateAdhesionLineData: Completed successfully\n";
}

void CellManager::renderAdhesionLines(glm::vec2 resolution, const Camera& camera, bool showAdhesionLines)
{
    if (!showAdhesionLines) {
        //std::cout << "Adhesion lines disabled by UI toggle\n";
        return;
    }
    
    if (totalAdhesionCount == 0) {
        //std::cout << "No adhesion connections to render (totalAdhesionCount = 0)\n";
        return;
    }

    //std::cout << "Rendering " << totalAdhesionCount << " adhesion connections\n";

    updateAdhesionLineData();

    TimerGPU timer("Adhesion Rendering");

    adhesionLineShader->use();

    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);

    adhesionLineShader->setMat4("uProjection", projection);
    adhesionLineShader->setMat4("uView", view);

    // Enable depth testing and depth writing for proper depth sorting with ring gizmos
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    // Enable line width for better visibility
    glLineWidth(4.0f);

    // Render adhesion lines - each connection has 2 line segments (4 vertices)
    glBindVertexArray(adhesionLineVAO);
    glDrawArrays(GL_LINES, 0, totalAdhesionCount * 4); // 4 vertices per connection (2 line segments)
    glBindVertexArray(0);
    glLineWidth(1.0f);
    
    //std::cout << "Adhesion lines rendered successfully\n";
}

void CellManager::cleanupAdhesionLines()
{
    if (adhesionLineBuffer != 0)
    {
        glDeleteBuffers(1, &adhesionLineBuffer);
        adhesionLineBuffer = 0;
    }
    if (adhesionLineVBO != 0)
    {
        glDeleteBuffers(1, &adhesionLineVBO);
        adhesionLineVBO = 0;
    }
    if (adhesionLineVAO != 0)
    {
        glDeleteVertexArrays(1, &adhesionLineVAO);
        adhesionLineVAO = 0;
    }
}

// ============================================================================
// ADHESION CONNECTION SYSTEM
// ============================================================================
void CellManager::initializeAdhesionConnectionSystem()
{
    int adhesionLimit = gpuMainMaxCapacity * config::MAX_ADHESIONS_PER_CELL / 2;
    std::cout << "Initializing adhesion connection system with capacity for " << adhesionLimit << " connections\n";
    
    // Create buffer for adhesion connections
    // Each connection stores: cellAIndex, cellBIndex, modeIndex, isActive, anchorDirectionA, paddingA, anchorDirectionB, paddingB, twistReferenceA, twistReferenceB
    // Total size: 4 uints + 2 vec3s + 2 floats + 2 quats = 4*4 + 2*12 + 2*4 + 2*16 = 16 + 24 + 8 + 32 = 80 bytes
    glCreateBuffers(1, &adhesionConnectionBuffer);
    glNamedBufferData(adhesionConnectionBuffer,
        adhesionLimit * sizeof(AdhesionConnection),
        nullptr, GL_DYNAMIC_READ);  // GPU produces data, CPU reads for connection count
    
    // Clear all existing adhesion connections to ensure clean state with new structure format
    // This prevents issues with old 48-byte data being interpreted as new 80-byte data
    totalAdhesionCount = 0;
    liveAdhesionCount = 0;
    
    std::cout << "Adhesion connection system initialized successfully\n";
    std::cout << "  - adhesionConnectionBuffer: " << adhesionConnectionBuffer << "\n";
    std::cout << "  - Buffer size: " << (adhesionLimit * sizeof(AdhesionConnection)) << " bytes\n";
    std::cout << "  - Structure size: " << sizeof(AdhesionConnection) << " bytes (was 48, now 80)\n";
    std::cout << "  - Current totalAdhesionCount: " << totalAdhesionCount << " (reset to 0)\n";
    std::cout << "  - Current totalCellCount: " << totalCellCount << "\n";
    std::cout << "  - Current liveCellCount: " << liveCellCount << "\n";
}


void CellManager::cleanupAdhesionConnectionSystem()
{
    if (adhesionConnectionBuffer != 0)
    {
        glDeleteBuffers(1, &adhesionConnectionBuffer);
        adhesionConnectionBuffer = 0;
    }
    totalAdhesionCount = 0;
}

// ============================================================================
// ADHESION CONNECTION KEYFRAME SUPPORT
// ============================================================================

std::vector<AdhesionConnection> CellManager::getAdhesionConnections() const
{
    std::vector<AdhesionConnection> connections;
    
    if (totalAdhesionCount == 0) {
        return connections;
    }
    
    // Ensure GPU operations are complete
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Create a staging buffer for adhesion connections
    GLuint stagingBuffer;
    glCreateBuffers(1, &stagingBuffer);
    glNamedBufferStorage(
        stagingBuffer,
        totalAdhesionCount * sizeof(AdhesionConnection),
        nullptr,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT | GL_DYNAMIC_STORAGE_BIT
    );
    
    // Copy adhesion connections from GPU to staging buffer
    glCopyNamedBufferSubData(adhesionConnectionBuffer, stagingBuffer, 0, 0, totalAdhesionCount * sizeof(AdhesionConnection));
    
    // Map the staging buffer for reading
    void* mappedPtr = glMapNamedBufferRange(stagingBuffer, 0, totalAdhesionCount * sizeof(AdhesionConnection),
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    
    if (mappedPtr) {
        AdhesionConnection* connectionData = static_cast<AdhesionConnection*>(mappedPtr);
        
        // Copy connections to vector
        connections.reserve(totalAdhesionCount);
        for (int i = 0; i < totalAdhesionCount; i++) {
            connections.push_back(connectionData[i]);
        }
        
        glUnmapNamedBuffer(stagingBuffer);
    }
    
    // Cleanup staging buffer
    glDeleteBuffers(1, &stagingBuffer);
    
    return connections;
}

void CellManager::restoreAdhesionConnections(const std::vector<AdhesionConnection> &connections, int count)
{
    if (count == 0) {
        totalAdhesionCount = 0;
        // Clear the adhesion connection buffer
        glClearNamedBufferData(adhesionConnectionBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
        return;
    }
    
    // Update adhesion count
    totalAdhesionCount = count;
    
    // Update the adhesion connection buffer
    glNamedBufferSubData(adhesionConnectionBuffer,
                         0,
                         count * sizeof(AdhesionConnection),
                         connections.data());
    
    // Update the GPU cell count buffer
    GLuint counts[4] = { static_cast<GLuint>(totalCellCount), static_cast<GLuint>(liveCellCount), static_cast<GLuint>(totalAdhesionCount), static_cast<GLuint>(liveAdhesionCount) };
    glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint) * 4, counts);
    
    // Ensure GPU buffers are synchronized
    addBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
}