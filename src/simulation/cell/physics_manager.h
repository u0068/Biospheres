#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../rendering/core/shader_class.h"
#include "../cell/common_structs.h"

struct CellPhysicsManager
{
    // Compute shaders for physics
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    Shader* internalUpdateShader = nullptr;
    Shader* cellAdditionShader = nullptr;
    Shader* adhesionPhysicsShader = nullptr;

    // Adhesion connection system
    GLuint adhesionConnectionBuffer{};  // Buffer storing permanent adhesion connections

    // Configuration
    float spawnRadius = 50.0f;

    // Constructor and destructor
    CellPhysicsManager();
    ~CellPhysicsManager();

    // Physics computation
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
    void runInternalUpdateCompute(float deltaTime);
    void runAdhesionPhysics();

    // Cell spawning
    void spawnCells(int count = 1000);

    // Adhesion system
    void initializeAdhesionConnectionSystem();
    void cleanupAdhesionConnectionSystem();

    // Getters
    float getSpawnRadius() const { return spawnRadius; }
}; 