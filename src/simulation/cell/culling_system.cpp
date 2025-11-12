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
    
    // Initialize distance fade rendering shaders (Phagocyte)
    distanceFadeShader = new Shader("shaders/rendering/cell_types/phagocyte/sphere_distance_fade.vert", 
                                   "shaders/rendering/cell_types/phagocyte/sphere_distance_fade.frag");
    
    // Initialize flagellocyte shader (no geometry shader)
    flagellocyteShader = new Shader("shaders/rendering/cell_types/flagellocyte/flagellocyte.vert",
                                    "shaders/rendering/cell_types/flagellocyte/flagellocyte.frag");
    
    // Initialize tail generation compute shader and rendering shader
    tailGenerateShader = new Shader("shaders/rendering/cell_types/flagellocyte/tail_generate.comp");
    tailRenderShader = new Shader("shaders/rendering/cell_types/flagellocyte/tail.vert",
                                  "shaders/rendering/cell_types/flagellocyte/tail.frag");
    
    // Create output buffers for each LOD level
    for (int i = 0; i < 4; i++) {
        glCreateBuffers(1, &unifiedOutputBuffers[i]);
        glNamedBufferStorage(
            unifiedOutputBuffers[i],
            gpuMainMaxCapacity * sizeof(float) * 16, // 4 vec4s per instance (positionAndRadius, color, orientation, fadeFactor)
            nullptr,
            GL_DYNAMIC_STORAGE_BIT
        );
        
        // Create separate buffers for flagellocyte cells
        glCreateBuffers(1, &flagellocyteOutputBuffers[i]);
        glNamedBufferStorage(
            flagellocyteOutputBuffers[i],
            gpuMainMaxCapacity * sizeof(float) * 16,
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
    
    // Create buffer for flagellocyte counts
    glCreateBuffers(1, &flagellocyteCountBuffer);
    glNamedBufferStorage(
        flagellocyteCountBuffer,
        sizeof(uint32_t) * 4,
        nullptr,
        GL_DYNAMIC_STORAGE_BIT | GL_MAP_READ_BIT
    );
    
    // Initialize tail rendering buffers (allocate for maximum segments)
    const int MAX_TAIL_SEGMENTS = 64;
    const int RADIAL_SEGMENTS = 8;
    const int CAP_SEGMENTS = 4;
    const int VERTICES_PER_CELL = (MAX_TAIL_SEGMENTS + 1) * RADIAL_SEGMENTS + (CAP_SEGMENTS * RADIAL_SEGMENTS) + 1; // +1 for tip
    const int BODY_INDICES = MAX_TAIL_SEGMENTS * RADIAL_SEGMENTS * 6; // 2 triangles per quad
    const int CAP_INDICES = (CAP_SEGMENTS - 1) * RADIAL_SEGMENTS * 6 + RADIAL_SEGMENTS * 3; // Quads + tip triangles
    const int INDICES_PER_CELL = BODY_INDICES + CAP_INDICES;
    
    tailVertexCount = gpuMainMaxCapacity * VERTICES_PER_CELL;
    tailIndexCount = gpuMainMaxCapacity * INDICES_PER_CELL;
    
    // Create tail vertex buffer
    glCreateBuffers(1, &tailVertexBuffer);
    glNamedBufferStorage(
        tailVertexBuffer,
        tailVertexCount * sizeof(float) * 12, // 3 vec4s per vertex (position, normal, color)
        nullptr,
        GL_DYNAMIC_STORAGE_BIT
    );
    
    // Generate tail indices (once, reused for all cells)
    std::vector<uint32_t> tailIndices;
    tailIndices.reserve(INDICES_PER_CELL);
    
    // Body indices (generate for maximum segments)
    for (int seg = 0; seg < MAX_TAIL_SEGMENTS; seg++) {
        for (int rad = 0; rad < RADIAL_SEGMENTS; rad++) {
            int nextRad = (rad + 1) % RADIAL_SEGMENTS;
            int baseIdx = seg * RADIAL_SEGMENTS;
            int nextSegIdx = (seg + 1) * RADIAL_SEGMENTS;
            
            // First triangle
            tailIndices.push_back(baseIdx + rad);
            tailIndices.push_back(nextSegIdx + rad);
            tailIndices.push_back(baseIdx + nextRad);
            
            // Second triangle
            tailIndices.push_back(baseIdx + nextRad);
            tailIndices.push_back(nextSegIdx + rad);
            tailIndices.push_back(nextSegIdx + nextRad);
        }
    }
    
    // Cap indices
    int capBaseIndex = (MAX_TAIL_SEGMENTS + 1) * RADIAL_SEGMENTS;
    int lastBodyRingIndex = MAX_TAIL_SEGMENTS * RADIAL_SEGMENTS;
    
    // Connect last body ring to first cap ring
    for (int rad = 0; rad < RADIAL_SEGMENTS; rad++) {
        int nextRad = (rad + 1) % RADIAL_SEGMENTS;
        
        tailIndices.push_back(lastBodyRingIndex + rad);
        tailIndices.push_back(capBaseIndex + rad);
        tailIndices.push_back(lastBodyRingIndex + nextRad);
        
        tailIndices.push_back(lastBodyRingIndex + nextRad);
        tailIndices.push_back(capBaseIndex + rad);
        tailIndices.push_back(capBaseIndex + nextRad);
    }
    
    // Cap rings
    for (int capSeg = 0; capSeg < CAP_SEGMENTS - 1; capSeg++) {
        for (int rad = 0; rad < RADIAL_SEGMENTS; rad++) {
            int nextRad = (rad + 1) % RADIAL_SEGMENTS;
            int baseIdx = capBaseIndex + capSeg * RADIAL_SEGMENTS;
            int nextSegIdx = capBaseIndex + (capSeg + 1) * RADIAL_SEGMENTS;
            
            tailIndices.push_back(baseIdx + rad);
            tailIndices.push_back(nextSegIdx + rad);
            tailIndices.push_back(baseIdx + nextRad);
            
            tailIndices.push_back(baseIdx + nextRad);
            tailIndices.push_back(nextSegIdx + rad);
            tailIndices.push_back(nextSegIdx + nextRad);
        }
    }
    
    // Connect last cap ring to tip vertex
    int tipIndex = capBaseIndex + CAP_SEGMENTS * RADIAL_SEGMENTS;
    int lastCapRingIndex = capBaseIndex + (CAP_SEGMENTS - 1) * RADIAL_SEGMENTS;
    
    for (int rad = 0; rad < RADIAL_SEGMENTS; rad++) {
        int nextRad = (rad + 1) % RADIAL_SEGMENTS;
        
        tailIndices.push_back(lastCapRingIndex + rad);
        tailIndices.push_back(tipIndex);
        tailIndices.push_back(lastCapRingIndex + nextRad);
    }
    
    // Create tail index buffer (single pattern, repeated per cell using base vertex)
    glCreateBuffers(1, &tailIndexBuffer);
    glNamedBufferStorage(
        tailIndexBuffer,
        tailIndices.size() * sizeof(uint32_t),
        tailIndices.data(),
        0
    );
    
    // Create tail VAO
    glCreateVertexArrays(1, &tailVAO);
    glBindVertexArray(tailVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, tailVertexBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tailIndexBuffer);
    
    // Position (vec4)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 48, (void*)0);
    
    // Normal (vec4)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 48, (void*)16);
    
    // Color (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 48, (void*)32);
    
    glBindVertexArray(0);
    
    std::cout << "Unified culling system initialized\n";
    std::cout << "Tail rendering system initialized with " << VERTICES_PER_CELL << " vertices per cell\n";
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
    
    if (flagellocyteShader) {
        flagellocyteShader->destroy();
        delete flagellocyteShader;
        flagellocyteShader = nullptr;
    }
    
    if (tailGenerateShader) {
        tailGenerateShader->destroy();
        delete tailGenerateShader;
        tailGenerateShader = nullptr;
    }
    
    if (tailRenderShader) {
        tailRenderShader->destroy();
        delete tailRenderShader;
        tailRenderShader = nullptr;
    }
    
    if (sphereSkinShader) {
        sphereSkinShader->destroy();
        delete sphereSkinShader;
        sphereSkinShader = nullptr;
    }
    
    if (worldSphereShader) {
        worldSphereShader->destroy();
        delete worldSphereShader;
        worldSphereShader = nullptr;
    }
    
    for (int i = 0; i < 4; i++) {
        if (unifiedOutputBuffers[i] != 0) {
            glDeleteBuffers(1, &unifiedOutputBuffers[i]);
            unifiedOutputBuffers[i] = 0;
        }
        if (flagellocyteOutputBuffers[i] != 0) {
            glDeleteBuffers(1, &flagellocyteOutputBuffers[i]);
            flagellocyteOutputBuffers[i] = 0;
        }
    }
    
    if (unifiedCountBuffer != 0) {
        glDeleteBuffers(1, &unifiedCountBuffer);
        unifiedCountBuffer = 0;
    }
    
    if (flagellocyteCountBuffer != 0) {
        glDeleteBuffers(1, &flagellocyteCountBuffer);
        flagellocyteCountBuffer = 0;
    }
    
    // Cleanup tail buffers
    if (tailVertexBuffer != 0) {
        glDeleteBuffers(1, &tailVertexBuffer);
        tailVertexBuffer = 0;
    }
    
    if (tailIndexBuffer != 0) {
        glDeleteBuffers(1, &tailIndexBuffer);
        tailIndexBuffer = 0;
    }
    
    if (tailVAO != 0) {
        glDeleteVertexArrays(1, &tailVAO);
        tailVAO = 0;
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
    
    // Set sphere culling uniforms
    unifiedCullShader->setFloat("u_sphereRadius", config::SPHERE_RADIUS);
    unifiedCullShader->setVec3("u_sphereCenter", config::SPHERE_CENTER);
    unifiedCullShader->setInt("u_enableSphereCulling", config::ENABLE_SPHERE_CULLING ? 1 : 0);
    
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
        
        // Check if we need to render tails (check if any mode in genome is flagellocyte)
        bool hasFlagellocyte = false;
        if (!currentGenome.modes.empty()) {
            for (const auto& mode : currentGenome.modes) {
                if (mode.cellType == CellType::Flagellocyte) {
                    hasFlagellocyte = true;
                    break;
                }
            }
        }
        
        // Debug output (only print once)
        static bool printedOnce = false;
        if (!printedOnce) {
            std::cout << "[DEBUG] Using standard shader\n";
            std::cout << "[DEBUG] Genome has " << currentGenome.modes.size() << " modes\n";
            if (!currentGenome.modes.empty()) {
                std::cout << "[DEBUG] Mode 0 type: " << static_cast<int>(currentGenome.modes[0].cellType) << " (0=Phagocyte, 1=Flagellocyte)\n";
                std::cout << "[DEBUG] Mode 0 color: (" << currentGenome.modes[0].color.r << ", " << currentGenome.modes[0].color.g << ", " << currentGenome.modes[0].color.b << ")\n";
            }
            printedOnce = true;
        }
        
        // Always use standard shader for cell bodies (both phagocytes and flagellocytes)
        Shader* activeShader = distanceFadeShader;
        activeShader->use();
        
        // Set up camera matrices
        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(config::defaultFrustumFov), aspectRatio, config::defaultFrustumNearPlane, config::defaultFrustumFarPlane);
        
        // Set uniforms
        activeShader->setMat4("uProjection", projection);
        activeShader->setMat4("uView", view);
        activeShader->setVec3("uCameraPos", camera.getPosition());
        activeShader->setVec3("uLightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
        
        // Set fog color for both shaders
        activeShader->setVec3("uFogColor", fogColor);
        
        // Selection highlighting uniforms
        if (selectedCell.isValid) {
            glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndMass);
            float selectedRadius = selectedCell.cellData.getRadius();
            activeShader->setVec3("uSelectedCellPos", selectedPos);
            activeShader->setFloat("uSelectedCellRadius", selectedRadius);
        } else {
            activeShader->setVec3("uSelectedCellPos", glm::vec3(-9999.0f));
            activeShader->setFloat("uSelectedCellRadius", 0.0f);
        }
        activeShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
        
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
        
        // Generate and render tails for flagellocyte cells
        if (hasFlagellocyte && tailGenerateShader && tailRenderShader && totalCellCount > 0) {
            // Use global settings
            const FlagellocyteSettings& settings = globalFlagellocyteSettings;
            
            // Run tail generation compute shader
            tailGenerateShader->use();
            tailGenerateShader->setInt("uCellCount", totalCellCount);
            tailGenerateShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
            tailGenerateShader->setVec3("uCameraPos", camera.getPosition());
            tailGenerateShader->setFloat("uFadeStartDistance", fadeStartDistance);
            tailGenerateShader->setFloat("uFadeEndDistance", fadeEndDistance);
            
            // Bind cell buffer, tail vertex buffer, and mode buffer (for colors)
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, tailVertexBuffer);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, modeBuffer);
            
            // Dispatch compute shader
            int workGroups = (totalCellCount + 63) / 64;
            glDispatchCompute(workGroups, 1, 1);
            glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);
            
            // Render tails
            tailRenderShader->use();
            tailRenderShader->setMat4("uProjection", projection);
            tailRenderShader->setMat4("uView", view);
            tailRenderShader->setVec3("uCameraPos", camera.getPosition());
            tailRenderShader->setVec3("uLightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
            tailRenderShader->setVec3("uFogColor", fogColor);
            
            glBindVertexArray(tailVAO);
            
            const int RADIAL_SEGMENTS = 8;
            const int CAP_SEGMENTS = 4;
            const int MAX_TAIL_SEGMENTS = 64;
            const int HARDCODED_SEGMENTS = 20; // Must match shader constant
            
            // Calculate indices to draw based on hardcoded segment count
            int actualSegments = HARDCODED_SEGMENTS;
            int actualVerticesPerCell = (actualSegments + 1) * RADIAL_SEGMENTS + (CAP_SEGMENTS * RADIAL_SEGMENTS) + 1;
            int actualBodyIndices = actualSegments * RADIAL_SEGMENTS * 6;
            int actualCapIndices = (CAP_SEGMENTS - 1) * RADIAL_SEGMENTS * 6 + RADIAL_SEGMENTS * 3;
            int actualIndicesPerCell = actualBodyIndices + actualCapIndices;
            
            // Maximum vertices per cell for base vertex offset
            int maxVerticesPerCell = (MAX_TAIL_SEGMENTS + 1) * RADIAL_SEGMENTS + (CAP_SEGMENTS * RADIAL_SEGMENTS) + 1;
            
            // Draw all tails (one draw call using base vertex for each cell)
            for (int i = 0; i < totalCellCount; i++) {
                glDrawElementsBaseVertex(GL_TRIANGLES, actualIndicesPerCell, GL_UNSIGNED_INT, 0, i * maxVerticesPerCell);
            }
            
            glBindVertexArray(0);
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

void CellManager::renderSphereSkin(const Camera& camera, glm::vec2 resolution)
{
    if (!runtimeSphereEnabled) return;
    
    // Initialize world sphere shader if needed
    if (!worldSphereShader) {
        worldSphereShader = new Shader("shaders/rendering/world_boundary/world_sphere.vert", 
                                       "shaders/rendering/world_boundary/world_sphere.frag");
    }
    
    // Ensure world sphere mesh is initialized with icosphere (smoother than lat/lon)
    if (worldSphereMesh.getIndexCount() == 0) {
        worldSphereMesh.generateIcosphere(0, 4, 1.0f); // Use icosphere for smoother geometry
        worldSphereMesh.setupBuffers();
    }
    
    worldSphereShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(config::defaultFrustumFov), 
                                           resolution.x / resolution.y,
                                           config::defaultFrustumNearPlane, 
                                           config::defaultFrustumFarPlane);
    
    // Create model matrix for sphere (position at sphere center, scale to sphere radius)
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, config::SPHERE_CENTER);
    model = glm::scale(model, glm::vec3(config::SPHERE_RADIUS));
    
    // Set uniforms using runtime parameters
    worldSphereShader->setMat4("uModel", model);
    worldSphereShader->setMat4("uView", view);
    worldSphereShader->setMat4("uProjection", projection);
    worldSphereShader->setVec3("uViewPos", camera.getPosition());
    worldSphereShader->setVec3("uLightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
    worldSphereShader->setVec3("uSphereColor", runtimeSphereColor);
    worldSphereShader->setFloat("uTransparency", runtimeSphereTransparency);
    worldSphereShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
    
    // Set distance fade uniforms using existing fade system
    worldSphereShader->setFloat("u_fadeStartDistance", fadeStartDistance);
    worldSphereShader->setFloat("u_fadeEndDistance", fadeEndDistance);
    
    // Disable blending for opaque sphere test
    if (runtimeSphereTransparency < 1.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
    
    // Fix inverted faces by culling front faces instead of back faces
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT); // Cull front faces to show the correct side
    
    // Enable depth testing and write to depth buffer for proper layering
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // Render world sphere
    worldSphereMesh.renderSingle();
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); // Restore to back face culling
    glDisable(GL_BLEND);
}