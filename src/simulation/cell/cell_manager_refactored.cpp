#include "cell_manager_refactored.h"
#include "../../rendering/camera/camera.h"
#include "../../core/config.h"
#include "../../ui/ui_manager.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#include <random>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../../utils/timer.h"

CellManagerRefactored::CellManagerRefactored()
{
    // Initialize barrier optimization system
    barrierBatch.setStats(&barrierStats);
    
    // Set up spatial grid with shader manager reference
    spatialGrid.setShaderManager(&shaderManager);
}

CellManagerRefactored::~CellManagerRefactored()
{
    cleanup();
}

void CellManagerRefactored::updateCells(float deltaTime)
{
    // Update spatial grid first (read from current read buffer, write to write buffer)
    updateSpatialGrid();
    
    // Run physics computation (read from current read buffer, write to write buffer)
    runPhysicsCompute(deltaTime);
    
    // Rotate buffers after physics computation (following buffer access rules)
    rotateBuffers();
    
    // Run update computation (read from new read buffer, write to new write buffer)
    runUpdateCompute(deltaTime);
    
    // Rotate buffers after update computation
    rotateBuffers();
    
    // Run internal update computation (read from new read buffer, write to new write buffer)
    runInternalUpdateCompute(deltaTime);
    
    // Rotate buffers after internal update computation
    rotateBuffers();
    
    // Apply cell additions if any are pending
    applyCellAdditions();
    
    // Update gizmo data
    gizmoSystem.updateGizmoData();
    gizmoSystem.updateRingGizmoData();
    gizmoSystem.updateAdhesionLineData();
    
    // Run adhesion physics
    gizmoSystem.runAdhesionPhysics();
}

void CellManagerRefactored::renderCells(glm::vec2 resolution, Shader &cellShader, Camera &camera, bool wireframe)
{
    // Update frustum for culling
    updateFrustum(camera, camera.getFOV(), resolution.x / resolution.y, camera.getNearPlane(), camera.getFarPlane());
    
    // Run unified culling (read from current read buffer, write to culling output buffers)
    runUnifiedCulling(camera);
    
    // Render cells using unified culling system
    renderCellsUnified(resolution, camera, wireframe);
}

void CellManagerRefactored::spawnCells(int count)
{
    if (count <= 0) return;

    std::vector<ComputeCell> newCells;
    newCells.reserve(count);

    // Random number generation for cell spawning
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> posDist(-bufferManager.getSpawnRadius(), bufferManager.getSpawnRadius());
    std::uniform_real_distribution<float> massDist(0.5f, 2.0f);
    std::uniform_real_distribution<float> velDist(-1.0f, 1.0f);

    for (int i = 0; i < count; i++)
    {
        ComputeCell cell;
        
        // Random position within spawn radius
        cell.positionAndMass = glm::vec4(
            posDist(gen),
            posDist(gen),
            posDist(gen),
            massDist(gen)
        );
        
        // Random velocity
        cell.velocity = glm::vec4(
            velDist(gen),
            velDist(gen),
            velDist(gen),
            0.0f
        );
        
        // Initialize other properties
        cell.acceleration = glm::vec4(0.0f);
        cell.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        cell.angularVelocity = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        cell.angularAcceleration = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        cell.signallingSubstances = glm::vec4(0.0f);
        cell.modeIndex = 0;
        cell.age = 0.0f;
        cell.toxins = 0.0f;
        cell.nitrates = 1.0f;
        
        newCells.push_back(cell);
    }

    // Add cells to GPU buffer
    addCellsToGPUBuffer(newCells);
} 