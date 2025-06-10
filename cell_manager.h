#pragma once
#include <vector>
#include "shader_class.h"
#include "input.h"
#include "glm.hpp"
#include <glad/glad.h>
#include "sphere_mesh.h"

// Forward declaration
class Camera;

// GPU compute cell structure matching the compute shader
struct ComputeCell {
    glm::vec4 positionAndRadius;  // x, y, z, radius
    glm::vec4 velocityAndMass;    // vx, vy, vz, mass
    glm::vec4 acceleration;       // ax, ay, az, unused
};

struct CellManager {
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates
      // GPU buffer objects
    GLuint cellBuffer = 0;           // SSBO for compute cell data
    GLuint instanceBuffer = 0;       // VBO for instance rendering data

    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // Asynchronous readback system for performance monitoring
    GLuint readbackBuffer = 0;       // Buffer for async GPU->CPU data transfer
    GLsync readbackFence = nullptr;  // Sync object for async operations
    bool readbackInProgress = false;
    float readbackCooldown = 0.0f;   // Timer to limit readback frequency
    static constexpr float READBACK_INTERVAL = 0.5f; // Readback every 0.5 seconds
    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    Shader* extractShader = nullptr;  // For extracting instance data efficiently

    // CPU-side storage for initialization and debugging
    std::vector<ComputeCell> cpuCells;
    int cell_count{ 0 };

    // Configuration
    static const int MAX_CELLS = 10000;
    static const int DEFAULT_CELL_COUNT = 100;
    float spawnRadius = 15.0f;  // Acts as both spawn area and containment barrier

    // Constructor and destructor
    CellManager();
    ~CellManager();
    // We declare functions in the struct, but we will define them in the cell_manager.cpp file.
// This is because when a file is edited, the compiler will also have to recompile all the files that include it.
// So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.
    void initializeGPUBuffers();
    void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader& cellShader, class Camera& camera);
    void addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius);
    void updateCells(float deltaTime);
    void cleanup();

    // Getter functions for debug information
    int getCellCount() const { return cell_count; }
    float getSpawnRadius() const { return spawnRadius; }

    // GPU pipeline status getters
    bool isReadbackInProgress() const { return readbackInProgress; }
    bool isReadbackSystemHealthy() const { return readbackBuffer != 0; }
    float getReadbackCooldown() const { return readbackCooldown; }
    // Performance testing function
    void setActiveCellCount(int count) {
        if (count <= cell_count && count >= 0) {
            // This allows reducing active cells for performance testing
            // without changing the actual cell count or buffer data
        }
    }

    // Asynchronous readback functions for performance monitoring
    void initializeReadbackSystem();
    void updateReadbackSystem(float deltaTime);
    void requestAsyncReadback();
    bool checkAsyncReadback(ComputeCell* outputData, int maxCells);
    void cleanupReadbackSystem();

private:
    void updateGPUBuffers();
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
};
