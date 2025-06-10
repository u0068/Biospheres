#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"
#include"shader_class.h"

#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm.hpp>
#include<algorithm>
#include<thread>
#include<chrono>

#include "cell_manager.h"
#include "glad_helpers.h"
#include "glfw_helpers.h"
#include "imgui_helpers.h"
#include "config.h"
#include "input.h"
#include "ui_manager.h"
#include "camera.h"
#include "timer.h"

// Simple OpenGL error checking function
void checkGLError(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "OpenGL error after " << operation << ": " << error << "\n";
    }
}

// GLFW error callback
void glfwErrorCallback(int error, const char* description) {
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

int main()
{
	// Set up error callback before initializing GLFW
	glfwSetErrorCallback(glfwErrorCallback);
	
	initGLFW();
	GLFWwindow* window = createWindow();
	initGLAD(window);
	// Load the sphere shader for instanced rendering
	Shader sphereShader("shaders/sphere.vert", "shaders/sphere.frag");

	const ImGuiIO& io = initImGui(window); // This also initialises ImGui io
	Input input;
	input.init(window);
	// Initialise the camera
	Camera camera(glm::vec3(0.0f, 0.0f, 5.0f)); // Start further back to see the cell
	// Initialise the UI manager // We dont have any ui to manage yet
	ToolState toolState;
	UIManager uiManager;	// Initialise cells
	CellManager cellManager;
	cellManager.spawnCells(); // Use default cell count from config

	// Timing variables
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;
	
	// Performance monitoring variables
	float lastPerfUpdate = 0.0f;
	float perfUpdateInterval = 0.25f; // Update every 250ms
	float displayFPS = 0.0f;
	float displayFrameTime = 0.0f;
	int frameCount = 0;
	float frameTimeAccumulator = 0.0f;
	
	// Window state tracking
	bool wasMinimized = false;
	bool isCurrentlyMinimized = false;
	int lastKnownWidth = 0;
	int lastKnownHeight = 0;
	
	// Main while loop
	while (!glfwWindowShouldClose(window))
	{		// Calculate delta time
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		
		// Check window state first - before any OpenGL operations
		int currentWidth, currentHeight;
		glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
		bool isMinimized = (currentWidth == 0 || currentHeight == 0) || glfwGetWindowAttrib(window, GLFW_ICONIFIED);
		
		// Handle minimize/restore transitions
		if (isMinimized && !wasMinimized) {
			// Just became minimized
			std::cout << "Window minimized, suspending rendering\n";
			wasMinimized = true;
			isCurrentlyMinimized = true;
		} else if (!isMinimized && wasMinimized) {
			// Just restored from minimize
			std::cout << "Window restored, resuming rendering\n";
			wasMinimized = false;
			isCurrentlyMinimized = false;
			// Give the driver a moment to stabilize
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		
		// If minimized, do minimal processing and skip to next frame
		if (isCurrentlyMinimized || isMinimized) {
			glfwPollEvents();
			glfwSwapBuffers(window);
			std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps limit when minimized
			continue;
		}
		
		// Store valid dimensions
		if (currentWidth > 0 && currentHeight > 0) {
			lastKnownWidth = currentWidth;
			lastKnownHeight = currentHeight;
		}
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
		
		// Use the valid dimensions we stored
		int width = lastKnownWidth;
		int height = lastKnownHeight;
		
		// Final safety check - if we still don't have valid dimensions, skip this frame
		if (width <= 0 || height <= 0) {
			glfwPollEvents();
			continue;
		}		//// First we do some init stuff
		/// Clear the framebuffer for proper 3D rendering
		/// Tell OpenGL a new frame is about to begin
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		
		// Set viewport with our validated dimensions
		try {
			glViewport(0, 0, width, height);
			checkGLError("glViewport");
			
			// Clear framebuffer once at the start of the frame
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			checkGLError("glClear");
		} catch (...) {
			// If OpenGL operations fail, skip this frame
			std::cerr << "OpenGL viewport/clear failed, skipping frame\n";
			glfwPollEvents();
			continue;
		}
		//// Then we handle input
		/// I should probably put this stuff in a separate function instead of having it in the main loop
		// Take care of all GLFW events
		glfwPollEvents();
		input.update();

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			// Handle camera input
			camera.processInput(input, deltaTime);			// Handle cell selection and dragging
			glm::vec2 mousePos = input.getMousePosition(false); // Get raw screen coordinates
			bool isLeftMousePressed = input.isMouseJustPressed(GLFW_MOUSE_BUTTON_LEFT);
			bool isLeftMouseDown = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
			float scrollDelta = input.getScrollDelta();
			
			cellManager.handleMouseInput(mousePos, glm::vec2(width, height), camera, 
			                           isLeftMousePressed, isLeftMouseDown, scrollDelta);
		}
		//// Then we handle cell simulation
		try {
			TimerGPU timer("Cell Physics Update");
			cellManager.updateCells(deltaTime);
		} catch (const std::exception& e) {
			std::cerr << "Exception in cell simulation: " << e.what() << "\n";
		} catch (...) {
			std::cerr << "Unknown exception in cell simulation\n";
		}

		//// Then we handle rendering
		try {
			TimerGPU timer("Cell Rendering Update");
			cellManager.renderCells(glm::vec2(width, height), sphereShader, camera);
			checkGLError("renderCells");
		} catch (const std::exception& e) {
			std::cerr << "Exception in cell rendering: " << e.what() << "\n";
		} catch (...) {
			std::cerr << "Unknown exception in cell rendering\n";
		}//// Then we handle ImGUI
		//ui.renderUI();
		
		// Cell Inspector and Selection UI
		uiManager.renderCellInspector(cellManager);
		uiManager.renderSelectionInfo(cellManager);
		
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
		ImGui::Text("Camera Controls:");
		ImGui::BulletText("WASD - Move");
		ImGui::BulletText("Q/E - Roll");
		ImGui::BulletText("Space/C - Up/Down");
		ImGui::BulletText("Right-click + Drag - Look");
		ImGui::Separator();
		ImGui::Text("Cell Interaction:");
		ImGui::BulletText("Left-click - Select cell");
		ImGui::BulletText("Left-click + Drag - Move selected cell");
		ImGui::BulletText("Scroll Wheel - Adjust drag distance");
		
		// Show current selection info
		if (cellManager.hasSelectedCell()) {
			ImGui::Separator();
			const auto& selection = cellManager.getSelectedCell();
			ImGui::Text("Selected: Cell #%d", selection.cellIndex);
			ImGui::Text("Drag Distance: %.1f", selection.dragDistance);
		}
		ImGui::End();

		ImGui::ShowDemoWindow();
		// Renders the ImGUI elements last, so that they are on top of everything else
		try {
			ImGui::Render();
			checkGLError("ImGui::Render");

			// Update and Render additional Platform Windows
			if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			checkGLError("ImGui_ImplOpenGL3_RenderDrawData");

			// Swap the back buffer with the front buffer, so that the rendered image is displayed on the screen
			glfwSwapBuffers(window);
			checkGLError("glfwSwapBuffers");
		} catch (const std::exception& e) {
			std::cerr << "Exception in ImGui/buffer swap: " << e.what() << "\n";
			// Try to recover by just swapping buffers
			try {
				glfwSwapBuffers(window);
			} catch (...) {
				// If even buffer swap fails, just continue to next frame
			}
		} catch (...) {
			std::cerr << "Unknown exception in ImGui/buffer swap\n";
			// Try to recover by just swapping buffers
			try {
				glfwSwapBuffers(window);
			} catch (...) {
				// If even buffer swap fails, just continue to next frame
			}
		}
	}

	// destroy and terminate everything before ending the ID
	shutdownImGui();
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
