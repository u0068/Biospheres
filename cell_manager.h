#pragma once
#include <vector>
#include "shader_class.h"
#include "input.h"
#include "glm.hpp"
#include <glad/glad.h>
#include "sphere_mesh.h"

// Forward declaration
class Camera;

//struct ToolState;

struct GPUPackedCell {
	/// This is the data that we will send to the GPU for rendering.
	/// Of course, this is a simplified version of the cell data.
	/// No need for velocity or mass here, just position and radius (and color later)
	/// Reminder that vec3 is 12 bytes in c++, but it is 16 bytes in GLSL.
	/// There are 3 ways to deal with this:
	/// 1. Instead of using vec3, we can use vec4 and store something in the 4th component.
	///		This is the most efficient use of space, but it requires us to use vec4 in the shader as well.
	/// 2. Instead of using vec3, we can use vec4 and 0.f in the 4th component.
	/// 3. We can use vec3 and pad it with a float to make it 16 bytes, but the float will not be used in the shader.
	/// Another reminder: the struct size must be a multiple of 16 bytes for optimal performance on the GPU.
	/// This may also need padding
	///								// Base Alignment	//Aligned Offset
	glm::vec4 positionAndRadius;	// 16 bytes			// 0 bytes		// x, y, z, radius
};

// GPU compute cell structure matching the compute shader
struct ComputeCell {
    glm::vec4 positionAndRadius;  // x, y, z, radius
    glm::vec4 velocityAndMass;    // vx, vy, vz, mass
    glm::vec4 acceleration;       // ax, ay, az, unused
};

struct CellManager {
	// Rather than having a class for each cell, we will have a single CellManager class that manages all cells.
	// That way we can leverage the power of data oriented design to optimize our rendering and updates.
	// This is because when the cpu loads data into the cache, it will load a whole cache line at once, which is usually 64 bytes.
	// So for example when it loads the position of a cell, it will also load the positions of then next few cells as well.
	// So it won't have to load the positions of each cell one by one, which would be very inefficient.
	// On second thought, since we are doing as much as possible on the GPU, we might not need to worry about this as much.
	// So at some point we might want to switch to a class for each cell, but for now we will keep it like this because it's not causing any issues.
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<glm::vec3> accelerations;
    std::vector<float> masses;
	std::vector<float> radii; // I might make radius depend on the mass later, but for now we will keep it simple
    int cell_count{0};
    
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

