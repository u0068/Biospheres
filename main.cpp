#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"
#include"shader_class.h"

#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm.hpp>
#include<algorithm>

#include "cell_manager.h"
#include "glad_helpers.h"
#include "glfw_helpers.h"
#include "imgui_helpers.h"
#include "config.h"
#include "fullscreen_quad.h"
#include "input.h"
#include "ui_manager.h"
#include "camera.h"

//// If you're a dev, please help me I have no idea what im doing

int main()
{
	initGLFW();
	GLFWwindow* window = createWindow();
	initGLAD(window);
	// This is the quad that we will render everything on
	initFullscreenQuad();
	// Load the sphere shader for instanced rendering
	Shader sphereShader("shaders/sphere.vert", "shaders/sphere.frag");

	const ImGuiIO& io = initImGui(window); // This also initialises ImGui io
	Input input;
	input.init(window);
	// Initialise the camera
	Camera camera(glm::vec3(0.0f, 0.0f, 5.0f)); // Start further back to see the cell

	// Initialise the UI manager // We dont have any ui to manage yet
	ToolState toolState;
	// Initialise cells
	CellManager cellManager;
		// Add multiple cells for testing instanced rendering
	for (int i = 0; i < 500; i++) // Increased from 100 to 500 for stress testing
	{
		// Add a cell with random position, velocity, mass and radius
		cellManager.addCell(
			glm::vec3(
				(rand() % 100 - 50) * 0.5f, // Random position -25 to 25
				(rand() % 100 - 50) * 0.5f, 
				(rand() % 100 - 50) * 0.5f
			), 
			glm::vec3(
				(rand() % 10 - 5) * 0.1f,   // Random velocity
				(rand() % 10 - 5) * 0.1f, 
				(rand() % 10 - 5) * 0.1f
			), 
			static_cast<float>(rand() % 10 + 1),        // Random mass between 1 and 10
			static_cast<float>(rand() % 3 + 1) * 0.5f   // Random radius between 0.5 and 2.0
		);
	}	// Timing variables
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;
	
	// Performance monitoring variables
	float lastPerfUpdate = 0.0f;
	float perfUpdateInterval = 0.25f; // Update every 250ms
	float displayFPS = 0.0f;
	float displayFrameTime = 0.0f;
	int frameCount = 0;
	float frameTimeAccumulator = 0.0f;
	
	// Main while loop
	while (!glfwWindowShouldClose(window))
	{		// Calculate delta time
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		// Update performance monitoring
		frameCount++;
		frameTimeAccumulator += deltaTime;
		
		// Update performance display every 250ms
		if (currentFrame - lastPerfUpdate >= perfUpdateInterval) {
			displayFPS = frameCount / (currentFrame - lastPerfUpdate);
			displayFrameTime = (frameTimeAccumulator / frameCount) * 1000.0f; // Convert to ms
			
			frameCount = 0;
			frameTimeAccumulator = 0.0f;
			lastPerfUpdate = currentFrame;
		}
		//// First we do some init stuff
		/// Clear the framebuffer for proper 3D rendering
		/// Tell OpenGL a new frame is about to begin
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		glViewport(0, 0, width, height);
		
		// Clear framebuffer once at the start of the frame
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		//// Then we handle input
		/// I should probably put this stuff in a separate function instead of having it in the main loop
		// Take care of all GLFW events
		glfwPollEvents();
		input.update();

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			// Handle camera input
			camera.processInput(input, deltaTime);
		}

		//// Then we handle cell simulation
		cellManager.updateCells(deltaTime);

		//// Then we handle rendering
		cellManager.renderCells(glm::vec2(width, height), sphereShader, camera);		//// Then we handle ImGUI
		//ui.renderUI();
		
		// Performance Monitor with readable update rate
		ImGui::Begin("Performance Monitor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		ImGui::Text("FPS: %.1f", displayFPS);
		ImGui::Text("Frame Time: %.3f ms", displayFrameTime);
		ImGui::Text("Cells: %d", cellManager.getCellCount());
		
		// Visual performance indicators
		float targetFPS = 60.0f;
		ImGui::Text("Performance:");
		ImGui::SameLine();
		if (displayFPS >= targetFPS) {
			ImGui::TextColored(ImVec4(0, 1, 0, 1), "GOOD");
		} else if (displayFPS >= 30.0f) {
			ImGui::TextColored(ImVec4(1, 1, 0, 1), "OK");
		} else {
			ImGui::TextColored(ImVec4(1, 0, 0, 1), "POOR");
		}
		
		// Technical details
		const char* renderer = (const char*)glGetString(GL_RENDERER);
		if (renderer) ImGui::Text("GPU: %s", renderer);
		ImGui::Text("Total triangles: ~%d", 192 * cellManager.getCellCount());
		ImGui::End();
		
		// Camera Controls
		ImGui::Begin("Camera & Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
		glm::vec3 camPos = camera.getPosition();
		ImGui::Text("Position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
		ImGui::Separator();
		ImGui::Text("Controls:");
		ImGui::BulletText("WASD - Move");
		ImGui::BulletText("Q/E - Roll");
		ImGui::BulletText("Space/C - Up/Down");
		ImGui::BulletText("Right-click + Drag - Look");
		ImGui::End();

		ImGui::ShowDemoWindow();

		// Renders the ImGUI elements last, so that they are on top of everything else
		ImGui::Render();

		// Update and Render additional Platform Windows
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());


		// Swap the back buffer with the front buffer, so that the rendered image is displayed on the screen
		glfwSwapBuffers(window);
	}

	// destroy and terminate everything before ending the ID
	destroyFullscreenQuad();
	shutdownImGui();
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
