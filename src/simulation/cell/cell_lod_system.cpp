#include "cell_lod_system.h"
#include "../../core/config.h"
#include "../../rendering/camera/camera.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

CellLODSystem::CellLODSystem()
{
    initializeLODSystem();
}

CellLODSystem::~CellLODSystem()
{
    cleanupLODSystem();
}

void CellLODSystem::initializeLODSystem()
{
    // Initialize LOD compute shader
    lodComputeShader = new Shader("shaders/rendering/sphere/sphere_lod.comp");
    
    // Initialize LOD vertex shader
    lodVertexShader = new Shader("shaders/rendering/sphere/sphere_lod.vert", "shaders/rendering/sphere/sphere_lod.frag");

    // Create instance buffers for each LOD level
    for (int i = 0; i < 4; i++)
    {
        glCreateBuffers(1, &lodInstanceBuffers[i]);
        glNamedBufferData(lodInstanceBuffers[i],
            config::MAX_CELLS * sizeof(glm::vec4) * 3, // positionAndRadius, color, orientation
            nullptr,
            GL_DYNAMIC_COPY
        );
        lodInstanceCounts[i] = 0;
    }

    // Create LOD count buffer
    glCreateBuffers(1, &lodCountBuffer);
    glNamedBufferData(lodCountBuffer,
        4 * sizeof(GLuint), // Count for each LOD level
        nullptr,
        GL_DYNAMIC_COPY
    );
}

void CellLODSystem::cleanupLODSystem()
{
    if (lodVertexShader)
    {
        lodVertexShader->destroy();
        delete lodVertexShader;
        lodVertexShader = nullptr;
    }
    if (lodComputeShader)
    {
        lodComputeShader->destroy();
        delete lodComputeShader;
        lodComputeShader = nullptr;
    }

    for (int i = 0; i < 4; i++)
    {
        if (lodInstanceBuffers[i] != 0)
        {
            glDeleteBuffers(1, &lodInstanceBuffers[i]);
            lodInstanceBuffers[i] = 0;
        }
        lodInstanceCounts[i] = 0;
    }

    if (lodCountBuffer != 0)
    {
        glDeleteBuffers(1, &lodCountBuffer);
        lodCountBuffer = 0;
    }

    // Invalidate cache
    invalidateStatisticsCache();
}

void CellLODSystem::updateLODLevels(const Camera& camera)
{
    if (!useLODSystem) return;

    // Update LOD distances based on camera position
    // This could be made more sophisticated based on camera zoom, etc.
    // For now, we'll use the default distances
}

void CellLODSystem::runLODCompute(const Camera& camera)
{
    if (!useLODSystem || !lodComputeShader) return;

    lodComputeShader->use();
    
    // Set camera position for distance calculations
    glm::vec3 cameraPos = camera.getPosition();
    lodComputeShader->setVec3("cameraPosition", cameraPos);
    
    // Set LOD distance thresholds
    lodComputeShader->setFloat("lodDistance0", lodDistances[0]);
    lodComputeShader->setFloat("lodDistance1", lodDistances[1]);
    lodComputeShader->setFloat("lodDistance2", lodDistances[2]);
    lodComputeShader->setFloat("lodDistance3", lodDistances[3]);

    // Dispatch compute shader
    int numGroups = (config::MAX_CELLS + 255) / 256;
    glDispatchCompute(numGroups, 1, 1);
    
    // Memory barrier to ensure LOD computation is complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Read back LOD counts
    GLuint counts[4];
    glGetNamedBufferSubData(lodCountBuffer, 0, 4 * sizeof(GLuint), counts);
    
    for (int i = 0; i < 4; i++)
    {
        lodInstanceCounts[i] = static_cast<int>(counts[i]);
    }

    // Invalidate statistics cache since LOD counts changed
    invalidateStatisticsCache();
}

void CellLODSystem::renderCellsLOD(glm::vec2 resolution, const Camera& camera, bool wireframe)
{
    if (!useLODSystem || !lodVertexShader) return;

    lodVertexShader->use();
    
    // Set camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix();
    lodVertexShader->setMat4("view", view);
    lodVertexShader->setMat4("projection", projection);
    
    // Set wireframe mode
    lodVertexShader->setBool("wireframe", wireframe);

    // Render each LOD level
    for (int i = 0; i < 4; i++)
    {
        if (lodInstanceCounts[i] > 0)
        {
            // Bind the instance buffer for this LOD level
            glBindBuffer(GL_ARRAY_BUFFER, lodInstanceBuffers[i]);
            
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
            glDrawArraysInstanced(GL_TRIANGLES, 0, 36, lodInstanceCounts[i]); // Assuming sphere has 36 vertices
            
            // Clean up vertex attributes
            glDisableVertexAttribArray(0);
            glDisableVertexAttribArray(1);
            glDisableVertexAttribArray(2);
        }
    }
}

int CellLODSystem::getTotalTriangleCount() const
{
    if (cachedTriangleCount >= 0) return cachedTriangleCount;

    // Calculate total triangles across all LOD levels
    // This is a simplified calculation - actual triangle counts would depend on sphere mesh complexity
    int totalTriangles = 0;
    for (int i = 0; i < 4; i++)
    {
        // Assuming each sphere has 12 triangles (simplified)
        totalTriangles += lodInstanceCounts[i] * 12;
    }
    
    cachedTriangleCount = totalTriangles;
    return totalTriangles;
}

int CellLODSystem::getTotalVertexCount() const
{
    if (cachedVertexCount >= 0) return cachedVertexCount;

    // Calculate total vertices across all LOD levels
    // This is a simplified calculation - actual vertex counts would depend on sphere mesh complexity
    int totalVertices = 0;
    for (int i = 0; i < 4; i++)
    {
        // Assuming each sphere has 36 vertices (simplified)
        totalVertices += lodInstanceCounts[i] * 36;
    }
    
    cachedVertexCount = totalVertices;
    return totalVertices;
} 