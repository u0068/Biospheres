#pragma once
#include <vector>
#include "shader_class.h"
#include "input.h"
#include "glm/glm.hpp"
#include <glad/glad.h>

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
	// GPU-based cell management using compute shaders
	// This replaces the CPU-based vectors with GPU buffer objects
	// The compute shaders handle physics calculations and position updates
    
    // GPU buffer objects
    GLuint cellBuffer = 0;           // SSBO for compute cell data
    GLuint renderBuffer = 0;         // SSBO for rendering data
    
    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    
    // CPU-side storage for initialization and debugging
    std::vector<ComputeCell> cpuCells;
    int cell_count{0};
    
    // Maximum number of cells (for buffer allocation)
    static const int MAX_CELLS = 10000;
    
    // Constructor and destructor
    CellManager();
    ~CellManager();
    
	// We declare functions in the struct, but we will define them in the cell_manager.cpp file.
	// This is because when a file is edited, the compiler will also have to recompile all the files that include it.
	// So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.
    void initializeGPUBuffers();
    void renderCells(glm::vec2 resolution, Shader cellShader, class Camera& camera);
    void addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius);
    void updateCells(float deltaTime);
    void cleanup();
    
private:
    void updateGPUBuffers();
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
};

