#pragma once
#include <chrono>
#include <iostream>
#include <string_view>
#include "imgui.h"
#include "glad/glad.h"
#include <unordered_map>
#include <string>
#include <algorithm>

float myMax(const float a, const float b); // Regular max isn't working for some reason??? so, I have to make my own

struct TimerStats {
	float lastTimeMs = 0.0f;
	float totalTimeMs = 0.0f;
	float averageTimeMs = 0.0f;
	float maxTimeMs = 0.0f;

	int tickCount = 0;
	
	void addSample(float timeMs) {
		lastTimeMs = timeMs;
		totalTimeMs += timeMs;
		maxTimeMs = myMax(maxTimeMs, timeMs);
		tickCount++;
	}

	void finalizeFrame() {
		if (tickCount > 0)
			averageTimeMs = totalTimeMs / tickCount;
		else
			averageTimeMs = 0.0f;

		//tickCount = 0;
		//totalTimeMs = 0.0f;
	}
};


class TimerManager {
public:
	static TimerManager& instance() {
		static TimerManager inst;
		return inst;
	}

	void addSample(const std::string& name, float timeMs) {
		timers[name].addSample(timeMs);
	}

	void finalizeFrame() {
		for (auto& [_, timer] : timers)
		{
			timer.finalizeFrame();
		}
	}

	void drawImGui() {
		ImGui::Begin("Performance Monitor");
		for (auto& [name, timer] : timers) {
			ImGui::Text("%s:	\n	Last %.3f ms \n	Avg %.3f ms \n	Max %.3f ms \n	Total %.3f ms \n	Ticks %d",
				name.c_str(), timer.lastTimeMs, timer.averageTimeMs, timer.maxTimeMs, timer.totalTimeMs, timer.tickCount);
			timer.tickCount = 0;
			timer.totalTimeMs = 0.0f;
		}
		ImGui::End();
	}

private:
	std::unordered_map<std::string, TimerStats> timers;
};

class TimerCPU {
public:
	TimerCPU(const char* name) : name(name) {
		start = std::chrono::high_resolution_clock::now();
	}

	~TimerCPU() {
		auto end = std::chrono::high_resolution_clock::now();
		float ms = std::chrono::duration<float, std::milli>(end - start).count();
		TimerManager::instance().addSample(name, ms);
	}

private:
	const char* name;
	std::chrono::high_resolution_clock::time_point start;
};

class TimerGPU {
public:
	TimerGPU(const char* name) : name(name) {
		glGenQueries(1, &query);
		glBeginQuery(GL_TIME_ELAPSED, query);
	}

	~TimerGPU() {
		glEndQuery(GL_TIME_ELAPSED);

		GLuint64 ns;
		glGetQueryObjectui64v(query, GL_QUERY_RESULT, &ns);
		glDeleteQueries(1, &query);

		float ms = ns * 1e-6f;
		TimerManager::instance().addSample(name, ms);
	}

private:
	GLuint query;
	const char* name;
};
