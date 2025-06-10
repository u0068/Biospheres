#pragma once
#include <vector>
#include "shader_class.h"
#include "input.h"
#include "glm/glm.hpp"

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

struct CellManager {
	// Rather than having a class for each cell, we will have a single CellManager class that manages all cells.
	// That way we can leverage the power of data oriented design to optimize our rendering and updates.
	// This is because when the cpu loads data into the cache, it will load a whole cache line at once, which is usually 64 bytes.
	// So for example when it loads the position of a cell, it will also load the positions of then next few cells as well.
	// So it won't have to load the positions of each cell one by one, which would be very inefficient.
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<glm::vec3> accelerations;
    std::vector<float> masses;
	std::vector<float> radii; // I might make radius depend on the mass later, but for now we will keep it simple
    int cell_count{0};
	// We declare functions in the struct, but we will define them in the cell_manager.cpp file.
	// This is because when a file is edited, the compiler will also have to recompile all the files that include it.
	// So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.
    void renderCells(glm::vec2 resolution, Shader cellShader, class Camera& camera);
    void addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius);
    void updateCells();
    void updateCellPositionsAndVelocities();
    void resolveCellCollisions();
};

