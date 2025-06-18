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

	// ========== Cell Simulation Configuration ==========
	constexpr int MAX_CELLS{200000};
	constexpr int MAX_COMMANDS{50000};
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
	inline bool showDemoWindow = true;
	inline bool playStartupJingle = false;
}
