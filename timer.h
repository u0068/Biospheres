#pragma once
#include <chrono>
#include <iostream>
#include <string_view>
#include "imgui.h"
#include "glad/glad.h"

class TimerCPU
{
public:
	TimerCPU(const char* name) : name(name)
	{
		start_timepoint = std::chrono::high_resolution_clock::now();
	}

	~TimerCPU()
	{
		Stop();
	}

	void Stop()
	{
		auto end_timepoint = std::chrono::high_resolution_clock::now();
		auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_timepoint - start_timepoint).count();
		ImGui::Begin("Performance Monitor");
		ImGui::Text("%s: %.3f ms", name, timeElapsed * 0.001f);
		ImGui::End();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> start_timepoint;
	const char* name;
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
	// IMPORTANT: The timer itself adds about 0.7 ms overhead, so that should also be considered when measuring performance.
	TimerGPU(const char* name) : name(name)
	{
		glGenQueries(1, &query);

		// Start GPU timer
		glBeginQuery(GL_TIME_ELAPSED, query);
	}

	~TimerGPU()
	{
		Stop();
	}

	void Stop()
	{
		// End GPU timer
		glEndQuery(GL_TIME_ELAPSED);

		// Wait for the query result to be available
		// Note: This can block the CPU until the GPU finishes processing the query.
		GLuint64 timeElapsed;
		glGetQueryObjectui64v(query, GL_QUERY_RESULT, &timeElapsed);

		ImGui::Begin("Performance Monitor");
		ImGui::Text("%s GPU Time: %.3f ms", name, timeElapsed * 1e-6f);
		ImGui::End();
	}

private:
	GLuint query;
	const char* name;
};

// Usage example:
// {
//		TimerGPU timer("MyFunction");
//		// Your code here
// }