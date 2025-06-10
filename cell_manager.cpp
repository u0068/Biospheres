#include "cell_manager.h"
#include "fullscreen_quad.h"
#include "ui_manager.h"
#include "camera.h"

void CellManager::renderCells(glm::vec2 resolution, Shader cellShader, Camera& camera)
{
    // Tell OpenGL which Shader Program we want to use
    cellShader.use(); // This is just a wrapper for glUseProgram(shaderProgram.ID);    // First we need to set all the uniforms that the shader needs
	// Reminder that the naming convention for uniforms in glsl is u_camelCase and in C++ is snake_case
    cellShader.setInt("u_cellCount", cell_count);
    cellShader.setVec2("u_resolution", resolution);
    
    // Set camera uniforms
    cellShader.setVec3("u_cameraPos", camera.getPosition());
    cellShader.setVec3("u_cameraFront", camera.getFront());
    cellShader.setVec3("u_cameraRight", camera.getRight());
    cellShader.setVec3("u_cameraUp", camera.getUp());

    // Then we need to pack the cell data into a format suitable for the GPU
	std::vector<GPUPackedCell> packedCells; // later maybe lets preallocate this vector to avoid reallocations. I will need to set up profiling first to see if it makes a difference
    for (int i = 0; i < cell_count; ++i) {
        packedCells.push_back({
            glm::vec4(positions[i], radii[i]),
            });
    }

    // Create or update the shader storage buffer object (SSBO)
    // This is where we will store the packed cell data and send it to the GPU
    GLuint cellSSBO;
    glGenBuffers(1, &cellSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, packedCells.size() * sizeof(GPUPackedCell), packedCells.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellSSBO); // binding = 0 in shader
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    renderFullscreenQuad();
}

void CellManager::addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius)
{
	// Reminder: the cell data must be added to all the data vectors, otherwise there will be a mismatch
    positions.push_back(position);
    velocities.push_back(velocity);
    accelerations.push_back(glm::vec3(0));
    masses.push_back(mass);
    radii.push_back(radius);
    cell_count += 1; // Increment the cell count
}

void CellManager::updateCells()
{
	// An assert will throw an error if we fucked up the data structure
    assert("Mismatched cell data vectors!"
		&& cell_count == positions.size()
		&& cell_count == velocities.size()
        && cell_count == accelerations.size()
        && cell_count == masses.size()
        && cell_count == radii.size());

    resolveCellCollisions();
    updateCellPositionsAndVelocities();
}

void CellManager::updateCellPositionsAndVelocities()
{
    for (int i = 0; i < cell_count; ++i) {
        // Update the position based on the velocity
        velocities[i] += accelerations[i]; // Update velocity based on acceleration
		velocities[i] *= 0.9f; // Apply some damping to the velocity, might make more complex drag calculations later
        positions[i] += velocities[i]; // Simple Euler integration for now
		accelerations[i] = glm::vec3(0.0); // Reset acceleration for the next frame, otherwise chaos ensues
    }
}

void CellManager::resolveCellCollisions()
{
	// Simple collision resolution: if two cells are too close, push them apart
    for (int i = 0; i < cell_count; ++i) {
        glm::vec3 force{};
        for (int j = 0; j < cell_count; ++j) {
            if (i == j) continue; // Skip self-collision
            glm::vec3 delta = positions[i] - positions[j];
            float distance = glm::length(delta);
            float minDistance = radii[i] + radii[j];
            if (distance < minDistance) {
                // Collision detected, resolve it
                force += delta * (minDistance - distance); // Push force is proportional to the overlap
            }
        }
        accelerations[i] += force / masses[i]; // Apply force to cell i
    }
}