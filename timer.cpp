#include "timer.h"


void TimerCPU::Stop()
{
	auto end_timepoint = std::chrono::high_resolution_clock::now();
	auto timeElapsed = std::chrono::duration_cast<std::chrono::microseconds>(end_timepoint - start_timepoint).count();

	float ms = timeElapsed * 0.001f; // Convert microseconds to milliseconds

	if (useConsole)
	{
		std::cout << name << " CPU Time: " << ms  << " ms\n";
	}
	else
	{
		ImGui::Begin("Performance Monitor");
		ImGui::Text("%s CPU Time: %.3f ms", name, ms);
		ImGui::End();
	}
}

void TimerGPU::Stop()
{
	// End GPU timer
	glEndQuery(GL_TIME_ELAPSED);

	// Wait for the query result to be available
	// Note: This can block the CPU until the GPU finishes processing the query.
	GLuint64 timeElapsed;
	glGetQueryObjectui64v(query, GL_QUERY_RESULT, &timeElapsed);

	float ms = timeElapsed * 1e-6f; // Convert nanoseconds to milliseconds

	if (useConsole)
	{
		std::cout << name << " GPU Time: " << ms << " ms\n";
	}
	else
	{
		ImGui::Begin("Performance Monitor");
		ImGui::Text("%s GPU Time: %.3f ms", name, ms);
		ImGui::End();
	}
}