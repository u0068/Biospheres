#include "cell_manager.h"
#include "camera.h"
#include <iostream>
#include <cassert>
#include <gtc/matrix_transform.hpp>

CellManager::CellManager() {
    // Generate sphere mesh
    sphereMesh.generateSphere(16, 32, 1.0f); // Lower poly count for better performance
    sphereMesh.setupBuffers();
    
    initializeGPUBuffers();
    
    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell_physics.comp");
    updateShader = new Shader("shaders/cell_update.comp");
}

CellManager::~CellManager() {
    cleanup();
}

void CellManager::initializeGPUBuffers() {
    // Create compute buffer for cell data
    glGenBuffers(1, &cellBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_CELLS * sizeof(ComputeCell), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Create instance buffer for rendering (contains position + radius)
    glGenBuffers(1, &instanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER, MAX_CELLS * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    // Setup the sphere mesh to use our instance buffer
    sphereMesh.setupInstanceBuffer(instanceBuffer);
    
    // Reserve CPU storage
    cpuCells.reserve(MAX_CELLS);
}

void CellManager::renderCells(glm::vec2 resolution, Shader& cellShader, Camera& camera) {
    if (cell_count == 0) return;
    
    // Copy position and radius data from compute buffer to instance buffer
    glBindBuffer(GL_COPY_READ_BUFFER, cellBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, instanceBuffer);
    
    // Copy position and radius data (first vec4 of each ComputeCell)
    for (int i = 0; i < cell_count; ++i) {
        GLintptr readOffset = i * sizeof(ComputeCell);  // Full ComputeCell offset
        GLintptr writeOffset = i * sizeof(glm::vec4);   // Instance data offset
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 
                           readOffset, writeOffset, sizeof(glm::vec4));
    }
    
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    
    // Use the sphere shader
    cellShader.use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
                                          resolution.x / resolution.y, 
                                          0.1f, 1000.0f);
    
    // Set uniforms
    cellShader.setMat4("uProjection", projection);
    cellShader.setMat4("uView", view);
    cellShader.setVec3("uCameraPos", camera.getPosition());
    cellShader.setVec3("uLightDir", glm::vec3(1.0f, 1.0f, 1.0f));
    
    // Enable depth testing for proper 3D rendering
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Render instanced spheres
    sphereMesh.render(cell_count);
}

void CellManager::addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius) {
    if (cell_count >= MAX_CELLS) {
        std::cout << "Warning: Maximum cell count reached!" << std::endl;
        return;
    }
    
    // Create new compute cell
    ComputeCell newCell;
    newCell.positionAndRadius = glm::vec4(position, radius);
    newCell.velocityAndMass = glm::vec4(velocity, mass);
    newCell.acceleration = glm::vec4(0.0f);
    
    // Add to CPU storage
    cpuCells.push_back(newCell);
    cell_count++;
    
    // Update GPU buffer
    updateGPUBuffers();
}

void CellManager::updateCells(float deltaTime) {
    if (cell_count == 0) return;
    
    // Run physics computation on GPU
    runPhysicsCompute(deltaTime);
    
    // Add memory barrier to ensure physics calculations are complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Run position/velocity update on GPU
    runUpdateCompute(deltaTime);
    
    // Add memory barrier to ensure all computations are complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::cleanup() {
    if (cellBuffer != 0) {
        glDeleteBuffers(1, &cellBuffer);
        cellBuffer = 0;
    }
    if (instanceBuffer != 0) {
        glDeleteBuffers(1, &instanceBuffer);
        instanceBuffer = 0;
    }
    if (physicsShader) {
        physicsShader->destroy();
        delete physicsShader;
        physicsShader = nullptr;
    }
    if (updateShader) {
        updateShader->destroy();
        delete updateShader;
        updateShader = nullptr;
    }
    
    sphereMesh.cleanup();
}

void CellManager::updateGPUBuffers() {
    // Upload CPU cell data to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, cell_count * sizeof(ComputeCell), cpuCells.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runPhysicsCompute(float deltaTime) {
    physicsShader->use();
    
    // Set uniforms
    physicsShader->setFloat("u_deltaTime", deltaTime);
    physicsShader->setInt("u_cellCount", cell_count);
    physicsShader->setFloat("u_damping", 0.98f);
    
    // Bind cell buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    
    // Dispatch compute shader
    // Use work groups of 64 threads each
    GLuint numGroups = (cell_count + 63) / 64; // Round up division
    physicsShader->dispatch(numGroups, 1, 1);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runUpdateCompute(float deltaTime) {
    updateShader->use();
    
    // Set uniforms
    updateShader->setFloat("u_deltaTime", deltaTime);
    updateShader->setInt("u_cellCount", cell_count);
    updateShader->setFloat("u_damping", 0.98f);
    
    // Bind cell buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (cell_count + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::spawnCells(int count) {
    for (int i = 0; i < count && cell_count < MAX_CELLS; ++i) {
        // Random position within spawn radius
        float angle1 = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float angle2 = static_cast<float>(rand()) / RAND_MAX * 3.14159f;
        float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;
        
        glm::vec3 position = glm::vec3(
            radius * sin(angle2) * cos(angle1),
            radius * cos(angle2),
            radius * sin(angle2) * sin(angle1)
        );
        
        // Random velocity
        glm::vec3 velocity = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f
        );
        
        // Random mass and radius
        float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        float cellRadius = 0.5f + static_cast<float>(rand()) / RAND_MAX * 1.0f;
        
        addCell(position, velocity, mass, cellRadius);
    }
}