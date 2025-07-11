#include "cell_gizmo_system.h"
#include "../../core/config.h"
#include "../../rendering/camera/camera.h"
#include "../../ui/ui_manager.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

CellGizmoSystem::CellGizmoSystem()
{
    initializeGizmoBuffers();
    initializeRingGizmoBuffers();
    initializeAdhesionLineBuffers();
    initializeAdhesionConnectionSystem();
}

CellGizmoSystem::~CellGizmoSystem()
{
    cleanupGizmos();
    cleanupRingGizmos();
    cleanupAdhesionLines();
    cleanupAdhesionConnectionSystem();
}

void CellGizmoSystem::initializeGizmoBuffers()
{
    // Initialize gizmo extract shader
    gizmoExtractShader = new Shader("shaders/rendering/debug/gizmo_extract.comp");
    
    // Initialize gizmo rendering shader
    gizmoShader = new Shader("shaders/rendering/debug/gizmo.vert", "shaders/rendering/debug/gizmo.frag");

    // Create gizmo buffer
    glCreateBuffers(1, &gizmoBuffer);
    glNamedBufferData(gizmoBuffer,
        config::MAX_CELLS * 6 * sizeof(glm::vec3), // 6 vertices per gizmo (2 lines)
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Create gizmo VAO and VBO
    glCreateVertexArrays(1, &gizmoVAO);
    glCreateBuffers(1, &gizmoVBO);
    
    glBindVertexArray(gizmoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, config::MAX_CELLS * 6 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    
    glBindVertexArray(0);
}

void CellGizmoSystem::updateGizmoData()
{
    if (!gizmoExtractShader) return;

    gizmoExtractShader->use();
    
    // Dispatch compute shader to generate gizmo data
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure gizmo data generation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellGizmoSystem::cleanupGizmos()
{
    if (gizmoExtractShader)
    {
        gizmoExtractShader->destroy();
        delete gizmoExtractShader;
        gizmoExtractShader = nullptr;
    }
    if (gizmoShader)
    {
        gizmoShader->destroy();
        delete gizmoShader;
        gizmoShader = nullptr;
    }

    if (gizmoBuffer != 0)
    {
        glDeleteBuffers(1, &gizmoBuffer);
        gizmoBuffer = 0;
    }
    if (gizmoVAO != 0)
    {
        glDeleteVertexArrays(1, &gizmoVAO);
        gizmoVAO = 0;
    }
    if (gizmoVBO != 0)
    {
        glDeleteBuffers(1, &gizmoVBO);
        gizmoVBO = 0;
    }
}

void CellGizmoSystem::renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos)
{
    if (!showGizmos || !gizmoShader) return;

    gizmoShader->use();
    
    // Set camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    gizmoShader->setMat4("view", view);
    gizmoShader->setMat4("projection", projection);
    
    // Bind gizmo VAO and render
    glBindVertexArray(gizmoVAO);
    glDrawArrays(GL_LINES, 0, config::MAX_CELLS * 6); // 6 vertices per gizmo
    glBindVertexArray(0);
}

void CellGizmoSystem::initializeRingGizmoBuffers()
{
    // Initialize ring gizmo extract shader
    ringGizmoExtractShader = new Shader("shaders/rendering/debug/ring_gizmo_extract.comp");
    
    // Initialize ring gizmo rendering shader
    ringGizmoShader = new Shader("shaders/rendering/debug/ring_gizmo.vert", "shaders/rendering/debug/ring_gizmo.frag");

    // Create ring gizmo buffer
    glCreateBuffers(1, &ringGizmoBuffer);
    glNamedBufferData(ringGizmoBuffer,
        config::MAX_CELLS * 64 * sizeof(glm::vec3), // 64 vertices per ring
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Create ring gizmo VAO and VBO
    glCreateVertexArrays(1, &ringGizmoVAO);
    glCreateBuffers(1, &ringGizmoVBO);
    
    glBindVertexArray(ringGizmoVAO);
    glBindBuffer(GL_ARRAY_BUFFER, ringGizmoVBO);
    glBufferData(GL_ARRAY_BUFFER, config::MAX_CELLS * 64 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    
    glBindVertexArray(0);
}

void CellGizmoSystem::updateRingGizmoData()
{
    if (!ringGizmoExtractShader) return;

    ringGizmoExtractShader->use();
    
    // Dispatch compute shader to generate ring gizmo data
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure ring gizmo data generation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellGizmoSystem::cleanupRingGizmos()
{
    if (ringGizmoExtractShader)
    {
        ringGizmoExtractShader->destroy();
        delete ringGizmoExtractShader;
        ringGizmoExtractShader = nullptr;
    }
    if (ringGizmoShader)
    {
        ringGizmoShader->destroy();
        delete ringGizmoShader;
        ringGizmoShader = nullptr;
    }

    if (ringGizmoBuffer != 0)
    {
        glDeleteBuffers(1, &ringGizmoBuffer);
        ringGizmoBuffer = 0;
    }
    if (ringGizmoVAO != 0)
    {
        glDeleteVertexArrays(1, &ringGizmoVAO);
        ringGizmoVAO = 0;
    }
    if (ringGizmoVBO != 0)
    {
        glDeleteBuffers(1, &ringGizmoVBO);
        ringGizmoVBO = 0;
    }
}

void CellGizmoSystem::renderRingGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager)
{
    if (!ringGizmoShader) return;

    ringGizmoShader->use();
    
    // Set camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    ringGizmoShader->setMat4("view", view);
    ringGizmoShader->setMat4("projection", projection);
    
    // Bind ring gizmo VAO and render
    glBindVertexArray(ringGizmoVAO);
    glDrawArrays(GL_LINE_LOOP, 0, config::MAX_CELLS * 64); // 64 vertices per ring
    glBindVertexArray(0);
}

void CellGizmoSystem::initializeAdhesionLineBuffers()
{
    // Initialize adhesion line extract shader
    adhesionLineExtractShader = new Shader("shaders/rendering/debug/adhesion_line_extract.comp");
    
    // Initialize adhesion line rendering shader
    adhesionLineShader = new Shader("shaders/rendering/debug/adhesion_line.vert", "shaders/rendering/debug/adhesion_line.frag");

    // Create adhesion line buffer
    glCreateBuffers(1, &adhesionLineBuffer);
    glNamedBufferData(adhesionLineBuffer,
        config::MAX_CELLS * 2 * sizeof(glm::vec3), // 2 vertices per line
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Create adhesion line VAO and VBO
    glCreateVertexArrays(1, &adhesionLineVAO);
    glCreateBuffers(1, &adhesionLineVBO);
    
    glBindVertexArray(adhesionLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, adhesionLineVBO);
    glBufferData(GL_ARRAY_BUFFER, config::MAX_CELLS * 2 * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    
    glBindVertexArray(0);
}

void CellGizmoSystem::updateAdhesionLineData()
{
    if (!adhesionLineExtractShader) return;

    adhesionLineExtractShader->use();
    
    // Dispatch compute shader to generate adhesion line data
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure adhesion line data generation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellGizmoSystem::cleanupAdhesionLines()
{
    if (adhesionLineExtractShader)
    {
        adhesionLineExtractShader->destroy();
        delete adhesionLineExtractShader;
        adhesionLineExtractShader = nullptr;
    }
    if (adhesionLineShader)
    {
        adhesionLineShader->destroy();
        delete adhesionLineShader;
        adhesionLineShader = nullptr;
    }

    if (adhesionLineBuffer != 0)
    {
        glDeleteBuffers(1, &adhesionLineBuffer);
        adhesionLineBuffer = 0;
    }
    if (adhesionLineVAO != 0)
    {
        glDeleteVertexArrays(1, &adhesionLineVAO);
        adhesionLineVAO = 0;
    }
    if (adhesionLineVBO != 0)
    {
        glDeleteBuffers(1, &adhesionLineVBO);
        adhesionLineVBO = 0;
    }
}

void CellGizmoSystem::renderAdhesionLines(glm::vec2 resolution, const Camera& camera, bool showAdhesionLines)
{
    if (!showAdhesionLines || !adhesionLineShader) return;

    adhesionLineShader->use();
    
    // Set camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    adhesionLineShader->setMat4("view", view);
    adhesionLineShader->setMat4("projection", projection);
    
    // Bind adhesion line VAO and render
    glBindVertexArray(adhesionLineVAO);
    glDrawArrays(GL_LINES, 0, config::MAX_CELLS * 2); // 2 vertices per line
    glBindVertexArray(0);
}

void CellGizmoSystem::initializeAdhesionConnectionSystem()
{
    // Initialize adhesion physics shader
    adhesionPhysicsShader = new Shader("shaders/cell/physics/adhesion_physics.comp");

    // Create adhesion connection buffer
    glCreateBuffers(1, &adhesionConnectionBuffer);
    glNamedBufferData(adhesionConnectionBuffer,
        config::MAX_CELLS * sizeof(AdhesionConnection),
        nullptr,
        GL_DYNAMIC_COPY
    );
}

void CellGizmoSystem::runAdhesionPhysics()
{
    if (!adhesionPhysicsShader) return;

    adhesionPhysicsShader->use();
    
    // Dispatch compute shader for adhesion physics
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure adhesion physics computation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellGizmoSystem::cleanupAdhesionConnectionSystem()
{
    if (adhesionPhysicsShader)
    {
        adhesionPhysicsShader->destroy();
        delete adhesionPhysicsShader;
        adhesionPhysicsShader = nullptr;
    }

    if (adhesionConnectionBuffer != 0)
    {
        glDeleteBuffers(1, &adhesionConnectionBuffer);
        adhesionConnectionBuffer = 0;
    }
} 