#pragma once
#include <chrono>
#include <iostream>
#include <string_view>
#include "imgui.h"
#include "glad/glad.h"

class TimerCPU
{
public:
	TimerCPU(const char* name, const bool useConsole = false) : name(name), useConsole(useConsole)
	{
		start_timepoint = std::chrono::high_resolution_clock::now();
	}

	~TimerCPU()
	{
		Stop();
	}

	void Stop();

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> start_timepoint;
	const char* name;
	const bool useConsole;
};

// Usage example:
// {
//		TimerCPU timer("MyFunction");
//		// Your code here
// }

class TimerGPU
{
public:
	// TimerGPU is designed to measure GPU performance using OpenGL queries.
	// IMPORTANT: You cannot use multiple TimerGPU instances simultaneously in the same thread.
	// IMPORTANT 2: The timer itself adds a small amount of overhead, so that should also be considered when measuring performance.
	TimerGPU(const char* name, const bool useConsole = false) : name(name), useConsole(useConsole)
	{
		glGenQueries(1, &query);

		// Start GPU timer
		glBeginQuery(GL_TIME_ELAPSED, query);
	}

	~TimerGPU()
	{
		Stop();
	}

	void Stop();

private:
	GLuint query;
	const char* name;
	const bool useConsole;
};

// Usage example:
// {
//		TimerGPU timer("MyFunction");
//		// Your code here
// }