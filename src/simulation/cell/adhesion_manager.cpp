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
    // Create buffer for adhesionSettings line vertices (each line has 2 vertices)
    // Each vertex has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &adhesionLineBuffer);
    glNamedBufferData(adhesionLineBuffer,
        cellLimit * config::MAX_ADHESIONS_PER_CELL * sizeof(glm::vec4) * 2, // 2 vertices per line, position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for adhesionSettings line rendering
    glCreateVertexArrays(1, &adhesionLineVAO);
    
    // Create VBO that will be bound to the adhesionSettings line buffer
    glCreateBuffers(1, &adhesionLineVBO);
    glNamedBufferData(adhesionLineVBO,
        cellLimit * config::MAX_ADHESIONS_PER_CELL * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(adhesionLineVAO, 0, adhesionLineVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(adhesionLineVAO, 0);
    glVertexArrayAttribFormat(adhesionLineVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(adhesionLineVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(adhesionLineVAO, 1);
    glVertexArrayAttribFormat(adhesionLineVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(adhesionLineVAO, 1, 0);
}

void CellManager::updateAdhesionLineData()
{
    if (totalAdhesionCount == 0) return;

    TimerGPU timer("Adhesion Data Update");

    adhesionLineExtractShader->use();

    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind adhesionSettings connection buffer as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, adhesionConnectionBuffer);
    // Bind adhesionSettings line buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adhesionLineBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);

    // Dispatch compute shader
    GLuint numGroups = (totalAdhesionCount + 63) / 64;
    adhesionLineExtractShader->dispatch(numGroups, 1, 1);

    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();

    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(adhesionLineBuffer, adhesionLineVBO, 0, 0, totalAdhesionCount * 2 * sizeof(glm::vec4) * 2);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderAdhesionLines(glm::vec2 resolution, const Camera& camera, bool showAdhesionLines)
{
    if (!showAdhesionLines || totalAdhesionCount == 0) return;

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

    // Render gizmo lines
    glBindVertexArray(adhesionLineVAO);
    glDrawArrays(GL_LINES, 0, totalAdhesionCount * 2); // 2 vertices per adhesionSettings
    glBindVertexArray(0);
    glLineWidth(1.0f);
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
    // Create buffer for adhesionSettings connections
    // Each connection stores: cellAIndex, cellBIndex, modeIndex, isActive (4 uints = 16 bytes)
    glCreateBuffers(1, &adhesionConnectionBuffer);
    glNamedBufferData(adhesionConnectionBuffer,
        cellLimit * config::MAX_ADHESIONS_PER_CELL * sizeof(AdhesionConnection) / 2,
        nullptr, GL_DYNAMIC_READ);  // GPU produces data, CPU reads for connection count
    
    std::cout << "Initialized adhesionSettings connection system with capacity for " << cellLimit * config::MAX_ADHESIONS_PER_CELL << " connections\n";
}

void CellManager::runAdhesionPhysics()
{
    if (totalCellCount == 0) return;
    
    TimerGPU timer("Adhesion Physics");
    
    adhesionPhysicsShader->use();
    
    // Set uniforms
    adhesionPhysicsShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    adhesionPhysicsShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    adhesionPhysicsShader->setFloat("u_worldSize", config::WORLD_SIZE);
    adhesionPhysicsShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID);
    adhesionPhysicsShader->setInt("u_maxConnections", cellLimit * config::MAX_ADHESIONS_PER_CELL);
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellWriteBuffer()); // Cell data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer); // Mode data
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridBuffer); // Spatial grid
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridCountBuffer); // Grid counts
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, adhesionConnectionBuffer); // Output connections
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gpuCellCountBuffer); // Cell count
    
    // Dispatch compute shader
    GLuint numGroups = (totalAdhesionCount + 255) / 256;
    adhesionPhysicsShader->dispatch(numGroups, 1, 1);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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