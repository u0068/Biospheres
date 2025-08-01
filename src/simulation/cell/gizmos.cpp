#include "cell_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../core/config.h"
#include "../../ui/ui_manager.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#include <cstdint>
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
    if (totalCellCount == 0) return;
    
    TimerGPU timer("Gizmo Data Update");
    
    gizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (totalCellCount + 63) / 64;
    gizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(gizmoBuffer, gizmoVBO, 0, 0, totalCellCount * 6 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos)
{
    if (!showGizmos || totalCellCount == 0) return;
    
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
    glDrawArrays(GL_LINES, 0, totalCellCount * 6); // 6 vertices per cell (3 lines * 2 vertices)
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

// ============================================================================
// RING GIZMO SYSTEM
// ============================================================================

void CellManager::initializeRingGizmoBuffers()
{
    // Create buffer for ring vertices (fixed size per cell)
    // Each cell produces: 2 rings (384 vertices)
    // Each vertex has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &ringGizmoBuffer);
    glNamedBufferData(ringGizmoBuffer,
        cellLimit * 384 * sizeof(glm::vec4) * 2, // position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for ring gizmo rendering
    glCreateVertexArrays(1, &ringGizmoVAO);
    
    // Create VBO that will be bound to the ring gizmo buffer
    glCreateBuffers(1, &ringGizmoVBO);
    glNamedBufferData(ringGizmoVBO,
        cellLimit * 384 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(ringGizmoVAO, 0, ringGizmoVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(ringGizmoVAO, 0);
    glVertexArrayAttribFormat(ringGizmoVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(ringGizmoVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(ringGizmoVAO, 1);
    glVertexArrayAttribFormat(ringGizmoVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(ringGizmoVAO, 1, 0);
}

void CellManager::initializeAnchorGizmoBuffers()
{
    // Create buffer for anchor instances (variable size - depends on number of adhesions)
    // Maximum possible anchors: cellLimit * 20 adhesions per cell
    // Each instance is AnchorInstance = 48 bytes (verified by static_assert)
    glCreateBuffers(1, &anchorGizmoBuffer);
    glNamedBufferData(anchorGizmoBuffer,
        cellLimit * 20 * sizeof(AnchorInstance), // Maximum possible anchor instances
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create instance VBO for rendering
    glCreateBuffers(1, &anchorGizmoVBO);
    glNamedBufferData(anchorGizmoVBO,
        cellLimit * 20 * sizeof(AnchorInstance),
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up the sphere mesh for instanced rendering (use the same mesh as cells)
    // The VAO will be set up when we render using the sphere mesh's setupInstanceBuffer
    
    // Create anchor count buffer
    glCreateBuffers(1, &anchorCountBuffer);
    glNamedBufferData(anchorCountBuffer,
        sizeof(uint32_t),
        nullptr, GL_DYNAMIC_COPY);
}

void CellManager::updateRingGizmoData()
{
    if (totalCellCount == 0) return;
    
    TimerGPU timer("Ring Gizmo Data Update");
    
    ringGizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind mode data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    // Bind ring gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ringGizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (totalCellCount + 63) / 64;
    ringGizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from compute buffer to VBO for rendering
    // Each cell has: 2 rings (384 vertices)
    glCopyNamedBufferSubData(ringGizmoBuffer, ringGizmoVBO, 0, 0, totalCellCount * 384 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderRingGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager)
{
    if (!uiManager.showOrientationGizmos || totalCellCount == 0) return;
    
    // Update ring gizmo data from current cell orientations and split directions
    updateRingGizmoData();
    
    TimerGPU timer("Ring Gizmo Rendering");
    
    ringGizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    ringGizmoShader->setMat4("uProjection", projection);
    ringGizmoShader->setMat4("uView", view);
    
    // Enable face culling so rings are only visible from one side
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable depth testing but disable depth writing to avoid z-fighting with spheres
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // Enable blending for better visibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render ring gizmo triangles
    glBindVertexArray(ringGizmoVAO);
    
    // Render all vertices in one call - the shader generates the correct number of vertices per cell
    // Each cell has: 2 rings (384 vertices)
    glDrawArrays(GL_TRIANGLES, 0, totalCellCount * 384);
    
    glBindVertexArray(0);
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

void CellManager::cleanupRingGizmos()
{
    if (ringGizmoBuffer != 0)
    {
        glDeleteBuffers(1, &ringGizmoBuffer);
        ringGizmoBuffer = 0;
    }
    if (ringGizmoVBO != 0)
    {
        glDeleteBuffers(1, &ringGizmoVBO);
        ringGizmoVBO = 0;
    }
    if (ringGizmoVAO != 0)
    {
        glDeleteVertexArrays(1, &ringGizmoVAO);
        ringGizmoVAO = 0;
    }
}

// ============================================================================
// ANCHOR GIZMO SYSTEM
// ============================================================================

void CellManager::updateAnchorGizmoData()
{
    if (totalCellCount == 0) return;
    
    TimerGPU timer("Anchor Gizmo Data Update");
    
    // Reset anchor count
    uint32_t zero = 0;
    glNamedBufferSubData(anchorCountBuffer, 0, sizeof(uint32_t), &zero);
    
    anchorGizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind mode data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    // Bind adhesion connection data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adhesionConnectionBuffer);
    // Bind anchor instance buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, anchorGizmoBuffer);
    // Bind anchor count buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, anchorCountBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (totalCellCount + 63) / 64;
    anchorGizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Get the total anchor count
    glGetNamedBufferSubData(anchorCountBuffer, 0, sizeof(uint32_t), &totalAnchorCount);
    
    // Debug output
    static uint32_t lastPrintedCount = 0;
    if (totalAnchorCount != lastPrintedCount) {
        std::cout << "Anchor gizmos: Found " << totalAnchorCount << " active adhesion anchors" << std::endl;
        lastPrintedCount = totalAnchorCount;
    }
    
    // Copy data from compute buffer to VBO for rendering
    if (totalAnchorCount > 0) {
        glCopyNamedBufferSubData(anchorGizmoBuffer, anchorGizmoVBO, 0, 0, totalAnchorCount * sizeof(AnchorInstance));
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderAnchorGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager)
{
    // Update anchor gizmo data first to get current count
    updateAnchorGizmoData();
    
    if (!uiManager.showOrientationGizmos) {
        return; // UI toggle is off
    }
    
    if (totalAnchorCount == 0) {
        return; // No anchors to render
    }
    
    // Debug output for rendering
    static bool printedRenderInfo = false;
    if (!printedRenderInfo && totalAnchorCount > 0) {
        std::cout << "Rendering " << totalAnchorCount << " anchor spheres with " << (totalAnchorCount * 72) << " vertices" << std::endl;
        printedRenderInfo = true;
    }
    
    TimerGPU timer("Anchor Gizmo Rendering");
    
    anchorGizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    anchorGizmoShader->setMat4("uProjection", projection);
    anchorGizmoShader->setMat4("uView", view);
    anchorGizmoShader->setVec3("uCameraPos", camera.getPosition());
    
    // Enable depth testing but disable depth writing to avoid z-fighting with spheres
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // Enable blending for better visibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set up sphere mesh for instanced rendering
    sphereMesh.setupInstanceBuffer(anchorGizmoVBO);
    
    // Render anchor gizmos using instanced sphere mesh
    sphereMesh.render(totalAnchorCount);
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void CellManager::cleanupAnchorGizmos()
{
    if (anchorGizmoBuffer != 0)
    {
        glDeleteBuffers(1, &anchorGizmoBuffer);
        anchorGizmoBuffer = 0;
    }
    if (anchorGizmoVBO != 0)
    {
        glDeleteBuffers(1, &anchorGizmoVBO);
        anchorGizmoVBO = 0;
    }
    // Note: anchorGizmoVAO is not created since we use the sphere mesh's VAO
    if (anchorCountBuffer != 0)
    {
        glDeleteBuffers(1, &anchorCountBuffer);
        anchorCountBuffer = 0;
    }
}