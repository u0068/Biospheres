#include "cell_culling_system.h"
#include "../../core/config.h"
#include "../../rendering/camera/camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

CellCullingSystem::CellCullingSystem()
{
    initializeUnifiedCulling();
}

CellCullingSystem::~CellCullingSystem()
{
    cleanupUnifiedCulling();
}

void CellCullingSystem::initializeUnifiedCulling()
{
    // Initialize unified culling compute shader
    unifiedCullShader = new Shader("shaders/rendering/culling/unified_cull.comp");
    
    // Initialize distance fade shaders
    distanceFadeShader = new Shader("shaders/rendering/sphere/sphere_distance_fade.vert", 
                                   "shaders/rendering/sphere/sphere_distance_fade.frag");

    // Create output buffers for each LOD level
    for (int i = 0; i < 4; i++)
    {
        glCreateBuffers(1, &unifiedOutputBuffers[i]);
        glNamedBufferData(unifiedOutputBuffers[i],
            config::MAX_CELLS * sizeof(glm::vec4) * 3, // positionAndRadius, color, orientation
            nullptr,
            GL_DYNAMIC_COPY
        );
    }

    // Create unified count buffer
    glCreateBuffers(1, &unifiedCountBuffer);
    glNamedBufferData(unifiedCountBuffer,
        4 * sizeof(GLuint), // Count for each LOD level
        nullptr,
        GL_DYNAMIC_COPY
    );

    // Initialize frustum
    currentFrustum = Frustum();
    visibleCellCount = 0;
}

void CellCullingSystem::cleanupUnifiedCulling()
{
    if (unifiedCullShader)
    {
        unifiedCullShader->destroy();
        delete unifiedCullShader;
        unifiedCullShader = nullptr;
    }
    if (distanceFadeShader)
    {
        distanceFadeShader->destroy();
        delete distanceFadeShader;
        distanceFadeShader = nullptr;
    }

    for (int i = 0; i < 4; i++)
    {
        if (unifiedOutputBuffers[i] != 0)
        {
            glDeleteBuffers(1, &unifiedOutputBuffers[i]);
            unifiedOutputBuffers[i] = 0;
        }
    }

    if (unifiedCountBuffer != 0)
    {
        glDeleteBuffers(1, &unifiedCountBuffer);
        unifiedCountBuffer = 0;
    }

    visibleCellCount = 0;
}

void CellCullingSystem::updateFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane)
{
    // Update frustum based on camera parameters
    currentFrustum.updateFrustum(camera.getViewMatrix(), camera.getProjectionMatrix());
}

void CellCullingSystem::runUnifiedCulling(const Camera& camera)
{
    if (!unifiedCullShader) return;

    unifiedCullShader->use();
    
    // Set camera position for distance calculations
    glm::vec3 cameraPos = camera.getPosition();
    unifiedCullShader->setVec3("cameraPosition", cameraPos);
    
    // Set culling parameters
    unifiedCullShader->setBool("useFrustumCulling", useFrustumCulling);
    unifiedCullShader->setBool("useDistanceCulling", useDistanceCulling);
    unifiedCullShader->setFloat("maxRenderDistance", maxRenderDistance);
    unifiedCullShader->setFloat("fadeStartDistance", fadeStartDistance);
    unifiedCullShader->setFloat("fadeEndDistance", fadeEndDistance);
    
    // Set frustum planes if frustum culling is enabled
    if (useFrustumCulling)
    {
        for (int i = 0; i < 6; i++)
        {
            unifiedCullShader->setVec4("frustumPlanes[" + std::to_string(i) + "]", currentFrustum.planes[i]);
        }
    }

    // Dispatch compute shader
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure culling computation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Read back visible cell count
    GLuint counts[4];
    glGetNamedBufferSubData(unifiedCountBuffer, 0, 4 * sizeof(GLuint), counts);
    
    visibleCellCount = 0;
    for (int i = 0; i < 4; i++)
    {
        visibleCellCount += static_cast<int>(counts[i]);
    }
}

void CellCullingSystem::renderCellsUnified(glm::vec2 resolution, const Camera& camera, bool wireframe)
{
    if (!distanceFadeShader) return;

    distanceFadeShader->use();
    
    // Set camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    distanceFadeShader->setMat4("view", view);
    distanceFadeShader->setMat4("projection", projection);
    
    // Set distance culling parameters
    distanceFadeShader->setFloat("maxRenderDistance", maxRenderDistance);
    distanceFadeShader->setFloat("fadeStartDistance", fadeStartDistance);
    distanceFadeShader->setFloat("fadeEndDistance", fadeEndDistance);
    distanceFadeShader->setVec3("fogColor", fogColor);
    distanceFadeShader->setVec3("cameraPosition", camera.getPosition());
    
    // Set wireframe mode
    distanceFadeShader->setBool("wireframe", wireframe);

    // Render each LOD level
    for (int i = 0; i < 4; i++)
    {
        // Get count for this LOD level
        GLuint count;
        glGetNamedBufferSubData(unifiedCountBuffer, i * sizeof(GLuint), sizeof(GLuint), &count);
        
        if (count > 0)
        {
            // Bind the output buffer for this LOD level
            glBindBuffer(GL_ARRAY_BUFFER, unifiedOutputBuffers[i]);
            
            // Set up vertex attributes for instanced rendering
            // Position and radius
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 3, (void*)0);
            glVertexAttribDivisor(0, 1);
            
            // Color
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 3, (void*)(sizeof(glm::vec4)));
            glVertexAttribDivisor(1, 1);
            
            // Orientation
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4) * 3, (void*)(sizeof(glm::vec4) * 2));
            glVertexAttribDivisor(2, 1);

            // Draw instanced
            glDrawArraysInstanced(GL_TRIANGLES, 0, 36, count); // Assuming sphere has 36 vertices
            
            // Clean up vertex attributes
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);
        }
    }
}

void CellCullingSystem::setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd)
{
    maxRenderDistance = maxDistance;
    fadeStartDistance = fadeStart;
    fadeEndDistance = fadeEnd;
} 