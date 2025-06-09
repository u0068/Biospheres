#pragma once
#include <vector>

#include "fullscreen_quad.h"
#include "shader_class.h"
#include "input.h"
#include "glm/glm.hpp"

//struct ToolState;

struct GPUPackedCell {
	// This is the data that we will send to the GPU for rendering.
	// Of course, this is a simplified version of the cell data.
	// No need for velocity or mass here, just position and radius (and color later)
	glm::vec4 positionAndRadius; // x, y, z, radius // We use vec4 to store position and radius together for better packing
};

struct CellManager {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<glm::vec3> accelerations;
    std::vector<float> masses;
    std::vector<float> radii;
    int cell_count{0};


    void renderCells(glm::vec2 resolution, Shader cellShader)
	{
        // Tell OpenGL which Shader Program we want to use
        cellShader.use(); // This is just a wrapper for glUseProgram(shaderProgram.ID);

		// First we need to set all the uniforms that the shader needs
		cellShader.setInt("u_cellCount", cell_count);
		cellShader.setVec2("u_resolution", resolution);
		// Later we will also need to set the camera position and view direction

		// Then we need to pack the cell data into a format suitable for the GPU
        std::vector<GPUPackedCell> packedCells;
        if (positions.size() != velocities.size() || positions.size() != masses.size() || positions.size() != radii.size()) {
            throw std::runtime_error("Mismatched sizes in cell data vectors");
        }
        for (size_t i = 0; i < cell_count; ++i) {
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

    void addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius)
	{
        positions.push_back(position);
		velocities.push_back(velocity);
        accelerations.push_back(glm::vec3(0));
        masses.push_back(mass);
		radii.push_back(radius);
		cell_count += 1; // Increment the cell count
	}

    void updateCells()
    {
        resolveCellCollisions();
        updateCellPositionsAndVelocities();
    }

    void updateCellPositionsAndVelocities()
    {
	    for (int i = 0; i < cell_count; ++i) {
            // Update the position based on the velocity
            velocities[i] += accelerations[i]; // Update velocity based on acceleration
			velocities[i] *= 0.9f; // Apply some damping to the velocity
            positions[i] += velocities[i]; // Simple Euler integration for now
            accelerations[i] = glm::vec3(0.0);
		}
    }

    void resolveCellCollisions()
    {
        for (int i = 0; i < cell_count; ++i) {
            glm::vec3 force{};
            for (int j = 0; j < cell_count; ++j) {
				if (i == j) continue; // Skip self-collision
                glm::vec3 delta = positions[i] - positions[j];
                float distance = glm::length(delta);
                float minDistance = radii[i] + radii[j];
                if (distance < minDistance) {
					// Collision detected, resolve it
					force += delta * (minDistance - distance); // Push apart proportional to the overlap
                }
			}
            accelerations[i] += force / masses[i]; // Apply force to cell i
        }
    }
};

