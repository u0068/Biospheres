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

void CellManager::initializeGizmoBuffers()
{
    // Create buffer for gizmo line vertices (each cell produces 6 vertices for 3 lines)
    // Each vertex now has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &gizmoBuffer);
    glNamedBufferData(gizmoBuffer,
        cellLimit * 6 * sizeof(glm::vec4) * 2, // position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for gizmo rendering
    glCreateVertexArrays(1, &gizmoVAO);
    
    // Create VBO that will be bound to the gizmo buffer
    glCreateBuffers(1, &gizmoVBO);
    glNamedBufferData(gizmoVBO,
        cellLimit * 6 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(gizmoVAO, 0, gizmoVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(gizmoVAO, 0);
    glVertexArrayAttribFormat(gizmoVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(gizmoVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(gizmoVAO, 1);
    glVertexArrayAttribFormat(gizmoVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(gizmoVAO, 1, 0);
}

void CellManager::updateGizmoData()
{
    if (cellCount == 0) return;
    
    TimerGPU timer("Gizmo Data Update");
    
    gizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    gizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(gizmoBuffer, gizmoVBO, 0, 0, cellCount * 6 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos)
{
    if (!showGizmos || cellCount == 0) return;
    
    // Update gizmo data from current cell orientations
    updateGizmoData();
    
    TimerGPU timer("Gizmo Rendering");
    
    gizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    gizmoShader->setMat4("uProjection", projection);
    gizmoShader->setMat4("uView", view);
    
    // Enable depth testing and depth writing for proper depth sorting with ring gizmos
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // Enable line width for better visibility
    glLineWidth(4.0f);
    
    // Render gizmo lines
    glBindVertexArray(gizmoVAO);
    glDrawArrays(GL_LINES, 0, cellCount * 6); // 6 vertices per cell (3 lines * 2 vertices)
    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void CellManager::cleanupGizmos()
{
    if (gizmoBuffer != 0)
    {
        glDeleteBuffers(1, &gizmoBuffer);
        gizmoBuffer = 0;
    }
    if (gizmoVBO != 0)
    {
        glDeleteBuffers(1, &gizmoVBO);
        gizmoVBO = 0;
    }
    if (gizmoVAO != 0)
    {
        glDeleteVertexArrays(1, &gizmoVAO);
        gizmoVAO = 0;
    }
}