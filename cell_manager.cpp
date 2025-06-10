#include "cell_manager.h"
#include "fullscreen_quad.h"
#include "ui_manager.h"
#include "camera.h"
#include <iostream>
#include <cassert>

CellManager::CellManager() {
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
    
    // Create render buffer for simplified rendering data
    glGenBuffers(1, &renderBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, MAX_CELLS * sizeof(GPUPackedCell), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Reserve CPU storage
    cpuCells.reserve(MAX_CELLS);
}

void CellManager::renderCells(glm::vec2 resolution, Shader cellShader, Camera& camera) {
    // Copy position and radius data from compute buffer to render buffer
    // This extracts only the data needed for rendering
    glBindBuffer(GL_COPY_READ_BUFFER, cellBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, renderBuffer);
    
    // Copy position and radius data (first vec4 of each ComputeCell)
    for (int i = 0; i < cell_count; ++i) {
        GLintptr readOffset = i * sizeof(ComputeCell);  // Full ComputeCell offset
        GLintptr writeOffset = i * sizeof(GPUPackedCell); // Packed cell offset
        glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 
                           readOffset, writeOffset, sizeof(glm::vec4));
    }
    
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    
    // Tell OpenGL which Shader Program we want to use
    cellShader.use();
    
    // Set uniforms
    cellShader.setInt("u_cellCount", cell_count);
    cellShader.setVec2("u_resolution", resolution);
    
    // Set camera uniforms
    cellShader.setVec3("u_cameraPos", camera.getPosition());
    cellShader.setVec3("u_cameraFront", camera.getFront());
    cellShader.setVec3("u_cameraRight", camera.getRight());
    cellShader.setVec3("u_cameraUp", camera.getUp());

    // Bind render buffer for fragment shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, renderBuffer);
    
    renderFullscreenQuad();
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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
    if (renderBuffer != 0) {
        glDeleteBuffers(1, &renderBuffer);
        renderBuffer = 0;
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