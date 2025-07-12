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

void CellManager::initializeLODSystem()
{
    // Initialize LOD shaders
    lodComputeShader = new Shader("shaders/rendering/sphere/sphere_lod.comp");
    lodVertexShader = new Shader("shaders/rendering/sphere/sphere_lod.vert", "shaders/rendering/sphere/sphere_lod.frag");
    
    // Generate LOD sphere meshes
    sphereMesh.generateLODSpheres(1.0f);
    sphereMesh.setupLODBuffers();
    
    // Create separate instance buffers for each LOD level
    glCreateBuffers(4, lodInstanceBuffers);
    for (int i = 0; i < 4; i++) {
        glNamedBufferStorage(
            lodInstanceBuffers[i],
            cellLimit * sizeof(float) * 12, // 3 vec4s per instance (positionAndRadius, color, orientation)
            nullptr,
            GL_DYNAMIC_STORAGE_BIT
        );
    }
    
    // Create LOD count buffer
    glCreateBuffers(1, &lodCountBuffer);
    glNamedBufferStorage(
        lodCountBuffer,
        4 * sizeof(uint32_t), // 4 LOD levels
        nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT
    );
    
    // Setup instance buffers for each LOD level
    sphereMesh.setupLODInstanceBuffers(lodInstanceBuffers);
    
    std::cout << "LOD system initialized with " << SphereMesh::LOD_LEVELS << " detail levels\n";
}

void CellManager::cleanupLODSystem()
{
    delete lodComputeShader;
    delete lodVertexShader;
    lodComputeShader = nullptr;
    lodVertexShader = nullptr;
    
    // Cleanup LOD instance buffers
    for (int i = 0; i < 4; i++) {
        if (lodInstanceBuffers[i] != 0) {
            glDeleteBuffers(1, &lodInstanceBuffers[i]);
            lodInstanceBuffers[i] = 0;
        }
    }
    
    // Cleanup LOD count buffer
    if (lodCountBuffer != 0) {
        glDeleteBuffers(1, &lodCountBuffer);
        lodCountBuffer = 0;
    }
}

void CellManager::runLODCompute(const Camera& camera)
{
    if (totalCellCount == 0) return;
    
    TimerGPU timer("LOD Instance Extraction");
    
    lodComputeShader->use();
    
    // Clear LOD count buffer before computation
    uint32_t zeroCounts[4] = {0, 0, 0, 0};
    glNamedBufferSubData(lodCountBuffer, 0, sizeof(zeroCounts), zeroCounts);
    
    // Set uniforms
    lodComputeShader->setVec3("u_cameraPos", camera.getPosition());
    lodComputeShader->setFloat("u_lodDistances[0]", lodDistances[0]);
    lodComputeShader->setFloat("u_lodDistances[1]", lodDistances[1]);
    lodComputeShader->setFloat("u_lodDistances[2]", lodDistances[2]);
    lodComputeShader->setFloat("u_lodDistances[3]", lodDistances[3]);
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, lodInstanceBuffers[0]); // LOD 0
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, lodInstanceBuffers[1]); // LOD 1
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, lodInstanceBuffers[2]); // LOD 2
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, lodInstanceBuffers[3]); // LOD 3
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, lodCountBuffer);        // LOD counts
    
    // Dispatch compute shader
    GLuint numGroups = (totalCellCount + 63) / 64;
    lodComputeShader->dispatch(numGroups, 1, 1);
    
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Read back LOD counts for rendering
    glGetNamedBufferSubData(lodCountBuffer, 0, sizeof(lodInstanceCounts), lodInstanceCounts);
    
    // Invalidate cache since LOD counts have changed
    invalidateStatisticsCache();
}

void CellManager::updateLODLevels(const Camera& camera)
{
    if (!useLODSystem || totalCellCount == 0) return;
    
    // Use unified culling system for all cases
    runUnifiedCulling(camera);
    
    flushBarriers();
}



int CellManager::getTotalTriangleCount() const {
    // Check if cache is valid
    if (cachedTriangleCount >= 0) {
        return cachedTriangleCount;
    }
    
    // Calculate and cache the result
    int totalTriangles = 0;
    
    // Unified culling system - use actual LOD distribution
    if (useFrustumCulling || useDistanceCulling || useLODSystem) {
        // Check if LOD system is using icospheres (which it should be)
        // For icospheres: 20 * 4^subdivisions triangles per sphere
        // Subdivisions: 3, 2, 1, 0 for LOD levels 0, 1, 2, 3
        int icosphereTriangles[4] = {1280, 320, 80, 20}; // 20 * 4^subdivisions
        
        for (int lod = 0; lod < 4; lod++) {
            // Account for back face culling - only front faces are rendered
            int visibleTriangles = icosphereTriangles[lod] / 2;
            totalTriangles += visibleTriangles * lodInstanceCounts[lod];
        }
    } else {
        // Fallback to old calculation for non-culling rendering
        // Legacy system uses latitude/longitude sphere: 8x12 segments = 96 triangles
        // Account for back face culling
        totalTriangles = 96 * totalCellCount; // 96 triangles per sphere, all visible
    }
    
    // Update cache
    cachedTriangleCount = totalTriangles;
    
    return totalTriangles;
}

int CellManager::getTotalVertexCount() const {
    // Check if cache is valid
    if (cachedVertexCount >= 0) {
        return cachedVertexCount;
    }
    
    // Calculate and cache the result
    int totalVertices = 0;
    
    // Unified culling system - use actual LOD distribution
    if (useFrustumCulling || useDistanceCulling || useLODSystem) {
        // Approximate icosphere vertex counts for each LOD level (subdivisions: 3, 2, 1, 0)
        int icosphereVertices[4] = {642, 162, 42, 12}; // Approximate vertex counts
        
        for (int lod = 0; lod < 4; lod++) {
            totalVertices += icosphereVertices[lod] * lodInstanceCounts[lod];
        }
    } else {
        // Fallback to old calculation for non-culling rendering
        // Legacy system uses latitude/longitude sphere: (8+1) * (12+1) = 9 * 13 = 117 vertices
        totalVertices = 117 * totalCellCount; // More accurate vertex count for 8x12 latitude/longitude sphere
    }
    
    // Update cache
    cachedVertexCount = totalVertices;
    
    return totalVertices;
}
