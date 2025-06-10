#pragma once
#include <string_view>


namespace config
{
	// This is where we put all the configuration constants, and many configurable variables.
	// This is also where we put the default values for the configuration variables.
	// And also where we define various "magic numbers" as constants to make the code clearer.
	constexpr int INITIAL_WINDOW_WIDTH{ 800 };
	constexpr int INITIAL_WINDOW_HEIGHT{ 600 };
	constexpr int OPENGL_VERSION_MAJOR{ 4 };
	constexpr int OPENGL_VERSION_MINOR{ 3 };
	constexpr const char* GLSL_VERSION{"#version 430"};
	constexpr const char* APPLICATION_NAME{ "Biospheres 2" };

	constexpr int MAX_CELLS{ 50000 };
	constexpr int DEFAULT_CELL_COUNT{ 1000 };
};

