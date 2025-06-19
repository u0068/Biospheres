#pragma once
#include <string_view>

namespace config
{
	// Application Configuration
	// This namespace contains all configuration constants, default values,
	// and "magic numbers" used throughout the application.

	// ========== Window and OpenGL Configuration ==========
	constexpr int INITIAL_WINDOW_WIDTH{800};
	constexpr int INITIAL_WINDOW_HEIGHT{600};
	constexpr int OPENGL_VERSION_MAJOR{4};
	constexpr int OPENGL_VERSION_MINOR{6};
	constexpr const char* GLSL_VERSION{"#version 460"};
	constexpr const char* APPLICATION_NAME{"Biospheres 2"};
	constexpr bool PLAY_STARTUP_JINGLE{false};

	// ========== Cell Simulation Configuration ==========
	constexpr int MAX_CELLS{100000};
	constexpr int DEFAULT_CELL_COUNT{100000};
	constexpr float DEFAULT_SPAWN_RADIUS{50.0f};

	// ========== Spatial Partitioning Configuration ==========
	constexpr float WORLD_SIZE{100.0f};                          // Size of the simulation world (cube from -50 to +50)
	constexpr int GRID_RESOLUTION{32};                            // Number of grid cells per dimension (32x32x32 = 32,768 total)
	constexpr float GRID_CELL_SIZE{WORLD_SIZE / GRID_RESOLUTION}; // Size of each grid cell
	constexpr int MAX_CELLS_PER_GRID{64};                         // Maximum cells per grid cell
	constexpr int TOTAL_GRID_CELLS{GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION};

	// ========== Runtime Configuration Variables ==========
	// These can be modified at runtime
	inline bool showDemoWindow{true};
	inline float physicsTimeStep{ 0.01f };	// The size of a physics time step, in simulation time
	inline float physicsSpeed{ 1.f };		// A multiplier on the physics tickrate. Physics tickrate = physicsSpeed / physicsTimeStep
	inline float maxAccumulatorTime{ 0.02f };// Maximum amount of time spent on simulating physics per frame. Max physics tpf = maxAccumulatorTime * tickrate
	inline float maxDeltaTime{ 0.1f };		// The maximum amount of time that can be accumulated by 1 frame
}
