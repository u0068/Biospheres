#pragma once
#include <string_view>

namespace config
{
	// This is where we put all the configuration constants, and many configurable variables.
	// This is also where we put the default values for the configuration variables.
	// And also where we define various "magic numbers" as constants to make the code clearer.

	// Window and OpenGL configuration
	constexpr int INITIAL_WINDOW_WIDTH{800};
	constexpr int INITIAL_WINDOW_HEIGHT{600};
	constexpr int OPENGL_VERSION_MAJOR{4};
	constexpr int OPENGL_VERSION_MINOR{6};
	constexpr const char *GLSL_VERSION{"#version 460"};
	constexpr const char *APPLICATION_NAME{"Biospheres 2"};

	// Cell simulation configuration
	constexpr int MAX_CELLS{200000};
	constexpr int MAX_COMMANDS{50000};
	constexpr int DEFAULT_CELL_COUNT{100000};
	constexpr float DEFAULT_SPAWN_RADIUS{15.0f};
	constexpr float READBACK_INTERVAL{0.5f}; // Async readback every 0.5 seconds

	// Spatial partitioning configuration
	constexpr float WORLD_SIZE{100.0f};							  // Size of the simulation world (cube from -50 to +50)
	constexpr int GRID_RESOLUTION{32};							  // Number of grid cells per dimension (32x32x32 = 32,768 total)
	constexpr float GRID_CELL_SIZE{WORLD_SIZE / GRID_RESOLUTION}; // Size of each grid cell
	constexpr int MAX_CELLS_PER_GRID{64};						  // Maximum cells per grid cell
	constexpr int TOTAL_GRID_CELLS{GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION};

	inline bool showDemoWindow = true;
	inline bool playStartupJingle = false;
};
