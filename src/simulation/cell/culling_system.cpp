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

void CellManager::initializeUnifiedCulling()
{
    // Initialize unified culling compute shader
    unifiedCullShader = new Shader("shaders/rendering/culling/unified_cull.comp");
    
    // Initialize distance fade rendering shaders
    distanceFadeShader = new Shader("shaders/rendering/sphere/sphere_distance_fade.vert", 
                                   "shaders/rendering/sphere/sphere_distance_fade.frag");
    
    // Create output buffers for each LOD level
    for (int i = 0; i < 4; i++) {
        glCreateBuffers(1, &unifiedOutputBuffers[i]);
        glNamedBufferStorage(
            unifiedOutputBuffers[i],
            cellLimit * sizeof(float) * 16, // 4 vec4s per instance (positionAndRadius, color, orientation, fadeFactor)
            nullptr,
            GL_DYNAMIC_STORAGE_BIT
        );
    }
    
    // Create buffer for LOD counts
    glCreateBuffers(1, &unifiedCountBuffer);
    glNamedBufferStorage(
        unifiedCountBuffer,
        sizeof(uint32_t) * 4, // 4 LOD levels
        nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT
    );
    
    std::cout << "Unified culling system initialized\n";
}

void CellManager::cleanupUnifiedCulling()
{
    if (unifiedCullShader) {
        unifiedCullShader->destroy();
        delete unifiedCullShader;
        unifiedCullShader = nullptr;
    }
    
    if (distanceFadeShader) {
        distanceFadeShader->destroy();
        delete distanceFadeShader;
        distanceFadeShader = nullptr;
    }
    
    for (int i = 0; i < 4; i++) {
        if (unifiedOutputBuffers[i] != 0) {
            glDeleteBuffers(1, &unifiedOutputBuffers[i]);
            unifiedOutputBuffers[i] = 0;
        }
    }
    
    if (unifiedCountBuffer != 0) {
        glDeleteBuffers(1, &unifiedCountBuffer);
        unifiedCountBuffer = 0;
    }
}

void CellManager::updateFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane)
{
    if (!useFrustumCulling) return;
    
    // Create frustum from camera parameters
    currentFrustum = FrustumCulling::createFrustum(camera, fov, aspectRatio, nearPlane, farPlane);
}

void CellManager::runUnifiedCulling(const Camera& camera)
{
    if (totalCellCount == 0) return;
    
    TimerGPU timer("Unified Culling");
    
    unifiedCullShader->use();
    
    // Clear count buffer before computation
    uint32_t zeroCounts[4] = {0, 0, 0, 0};
    glNamedBufferSubData(unifiedCountBuffer, 0, sizeof(zeroCounts), zeroCounts);
    
    // Set camera and distance culling uniforms
    unifiedCullShader->setVec3("u_cameraPos", camera.getPosition());
    unifiedCullShader->setFloat("u_maxRenderDistance", maxRenderDistance);
    unifiedCullShader->setFloat("u_fadeStartDistance", fadeStartDistance);
    unifiedCullShader->setFloat("u_fadeEndDistance", fadeEndDistance);
    
    // Set LOD distance uniforms
    unifiedCullShader->setFloat("u_lodDistances[0]", lodDistances[0]);
    unifiedCullShader->setFloat("u_lodDistances[1]", lodDistances[1]);
    unifiedCullShader->setFloat("u_lodDistances[2]", lodDistances[2]);
    unifiedCullShader->setFloat("u_lodDistances[3]", lodDistances[3]);
    
    // Set mode control uniforms (using setInt since setBool is not available)
    unifiedCullShader->setInt("u_useDistanceCulling", useDistanceCulling ? 1 : 0);
    unifiedCullShader->setInt("u_useLOD", useLODSystem ? 1 : 0);
    unifiedCullShader->setInt("u_useFade", useDistanceCulling ? 1 : 0);
    
    // Set frustum planes as uniforms
    const auto& planes = currentFrustum.getPlanes();
    for (int i = 0; i < 6; i++) {
        std::string uniformName = "u_frustumPlanes[" + std::to_string(i) + "]";
        unifiedCullShader->setVec3(uniformName + ".normal", planes[i].normal);
        unifiedCullShader->setFloat(uniformName + ".distance", planes[i].distance);
    }
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, unifiedOutputBuffers[0]); // LOD 0
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, unifiedOutputBuffers[1]); // LOD 1
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, unifiedOutputBuffers[2]); // LOD 2
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, unifiedOutputBuffers[3]); // LOD 3
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, unifiedCountBuffer);      // LOD counts
    
    // Dispatch compute shader
    GLuint numGroups = (totalCellCount + 63) / 64;
    unifiedCullShader->dispatch(numGroups, 1, 1);
    
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Read back LOD counts for rendering
    glGetNamedBufferSubData(unifiedCountBuffer, 0, sizeof(lodInstanceCounts), lodInstanceCounts);
    
    // Invalidate cache since LOD counts have changed
    invalidateStatisticsCache();
    
    // Calculate total visible cells for statistics
    visibleCellCount = lodInstanceCounts[0] + lodInstanceCounts[1] + lodInstanceCounts[2] + lodInstanceCounts[3];
}

void CellManager::renderCellsUnified(glm::vec2 resolution, const Camera& camera, bool wireframe)
{
    if (totalCellCount == 0) {
        return;
    }

    // Safety checks
    if (resolution.x <= 0 || resolution.y <= 0 || resolution.x < 1 || resolution.y < 1) {
        return;
    }

    try {
        // Update frustum for culling
        float aspectRatio = resolution.x / resolution.y;
        if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
            aspectRatio = 16.0f / 9.0f;
        }
        updateFrustum(camera, config::defaultFrustumFov, aspectRatio, config::defaultFrustumNearPlane, config::defaultFrustumFarPlane);
        
        // Run unified culling
        runUnifiedCulling(camera);
        
        TimerGPU timer("Unified Cell Rendering");
        
        // Use distance fade shader
        distanceFadeShader->use();
        
        // Set up camera matrices
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(config::defaultFrustumFov), aspectRatio, config::defaultFrustumNearPlane, config::defaultFrustumFarPlane);
        
        // Set uniforms
        distanceFadeShader->setMat4("uProjection", projection);
        distanceFadeShader->setMat4("uView", view);
        distanceFadeShader->setVec3("uCameraPos", camera.getPosition());
        distanceFadeShader->setVec3("uLightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
        distanceFadeShader->setVec3("uFogColor", fogColor);
        
        // Selection highlighting uniforms
        if (selectedCell.isValid) {
            glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndMass);
            float selectedRadius = selectedCell.cellData.getRadius();
            distanceFadeShader->setVec3("uSelectedCellPos", selectedPos);
            distanceFadeShader->setFloat("uSelectedCellRadius", selectedRadius);
        } else {
            distanceFadeShader->setVec3("uSelectedCellPos", glm::vec3(-9999.0f));
            distanceFadeShader->setFloat("uSelectedCellRadius", 0.0f);
        }
        distanceFadeShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
        
        // Enable depth testing (no blending needed since we're not using transparency)
        glEnable(GL_DEPTH_TEST);
        
        // Enable back face culling for better performance
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
        
        // Set polygon mode based on wireframe flag
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        
        // Render each LOD level with its appropriate mesh detail and instance data
        for (int lodLevel = 0; lodLevel < 4; lodLevel++) {
            if (lodInstanceCounts[lodLevel] > 0) {
                // Setup sphere mesh to use the unified output buffer with fade factor
                sphereMesh.setupLODInstanceBufferWithFade(lodLevel, unifiedOutputBuffers[lodLevel]);
                
                // Render this LOD level with its specific mesh detail and instance count
                sphereMesh.renderLOD(lodLevel, lodInstanceCounts[lodLevel], 0);
            }
        }
        
        // Restore OpenGL state
        glDisable(GL_CULL_FACE);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        
    } catch (const std::exception &e) {
        std::cerr << "Exception in renderCellsUnified: " << e.what() << "\n";
        // Fall back to regular rendering
        useFrustumCulling = false;
        useDistanceCulling = false;
        useLODSystem = false;
        invalidateStatisticsCache(); // Invalidate cache since culling settings changed
    } catch (...) {
        std::cerr << "Unknown exception in renderCellsUnified\n";
        useFrustumCulling = false;
        useDistanceCulling = false;
        useLODSystem = false;
        invalidateStatisticsCache(); // Invalidate cache since culling settings changed
    }
}



void CellManager::setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd)
{
    maxRenderDistance = maxDistance;
    fadeStartDistance = fadeStart;
    fadeEndDistance = fadeEnd;
}