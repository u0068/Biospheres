#pragma once
#include <chrono>
#include <iostream>
#include <string_view>
#include "imgui.h"

class Timer
{
public:
	Timer(const char* name) : name(name)
	{
		start_timepoint = std::chrono::high_resolution_clock::now();
	}

	~Timer()
	{
		Stop();
	}

	void Stop()
	{
		auto end_timepoint = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_timepoint - start_timepoint).count();
		ImGui::Begin("Performance Monitor");
		ImGui::Text("%s: %.3f ms", name, duration * 0.001f);
		ImGui::End();
	}

private:
	std::chrono::time_point<std::chrono::high_resolution_clock> start_timepoint;
	const char* name;
};

// Usage example:
// {
//		Timer timer("MyFunction");
//		// Your code here
// }
