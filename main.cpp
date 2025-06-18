#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "shader_class.h"

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <thread>
#include <chrono>

#define MINIAUDIO_IMPLEMENTATION
#include "audio_engine.h"
#include "cell_manager.h"
#include "glad_helpers.h"
#include "glfw_helpers.h"
#include "imgui_helpers.h"
#include "config.h"
#include "input.h"
#include "ui_manager.h"
#include "camera.h"
#include "timer.h"
#include "synthesizer.h"

// Simple OpenGL error checking function
void checkGLError(const char *operation)
{
	GLenum error = glGetError();
	if (error != GL_NO_ERROR)
	{
		std::cerr << "OpenGL error after " << operation << ": " << error << "\n";
	}
}

// GLFW error callback
void glfwErrorCallback(int error, const char *description)
{
	std::cerr << "GLFW Error " << error << ": " << description << "\n";
}

int main()
{
	{ // This scope is used to ensure the opengl elements are destroyed before the opengl context
		// Set up error callback before initializing GLFW
		glfwSetErrorCallback(glfwErrorCallback);
		initGLFW();
		GLFWwindow *window = createWindow();
		initGLAD(window);
		setupGLFWDebugFlags();
		// Load the sphere shader for instanced rendering
		Shader sphereShader("shaders/sphere.vert", "shaders/sphere.frag");

		const ImGuiIO &io = initImGui(window); // This also initialises ImGui io
		Input input;
		input.init(window);
		// Initialise the camera
		Camera camera(glm::vec3(0.0f, 0.0f, 10.0f)); // Start further back to see more cells
		// Initialise the UI manager // We dont have any ui to manage yet
		ToolState toolState;
		UIManager uiManager; // Initialise cells
		CellManager cellManager;
		cellManager.addGenomeToBuffer(uiManager.currentGenome);
		cellManager.spawnCells(config::MAX_CELLS);
		//cellManager.addCellToStagingBuffer(ComputeCell()); // spawns 1 cell at 0,0,0

		AudioEngine audioEngine;
		audioEngine.init();
		audioEngine.start();
		SynthEngine synthEngine;

		// Timing variables
		float deltaTime = 0.0f;
		float lastFrame = 0.0f;

		// Performance monitoring struct
		PerformanceMonitor perfMonitor{};

		// Window state tracking
		bool wasMinimized = false;
		bool isCurrentlyMinimized = false;
		int lastKnownWidth = 0;
		int lastKnownHeight = 0;

		// Main while loop
		while (!glfwWindowShouldClose(window))
		{ // Calculate delta time
			float currentFrame = static_cast<float>(glfwGetTime());
			deltaTime = currentFrame - lastFrame;
			lastFrame = currentFrame;

			// Check window state first - before any OpenGL operations
			int currentWidth, currentHeight;
			glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
			bool isMinimized = (currentWidth == 0 || currentHeight == 0) || glfwGetWindowAttrib(window, GLFW_ICONIFIED);

			// Handle minimize/restore transitions
			if (isMinimized && !wasMinimized)
			{
				// Just became minimized
				std::cout << "Window minimized, suspending rendering\n";
				wasMinimized = true;
				isCurrentlyMinimized = true;
			}
			else if (!isMinimized && wasMinimized)
			{
				// Just restored from minimize
				std::cout << "Window restored, resuming rendering\n";
				wasMinimized = false;
				isCurrentlyMinimized = false;
				// Give the driver a moment to stabilize
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}

			// If minimized, do minimal processing and skip to next frame
			if (isCurrentlyMinimized || isMinimized)
			{
				glfwPollEvents();
				glfwSwapBuffers(window);
				std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60fps limit when minimized
				continue;
			}

			// Store valid dimensions
			if (currentWidth > 0 && currentHeight > 0)
			{
				lastKnownWidth = currentWidth;
				lastKnownHeight = currentHeight;
			} // Update performance monitoring
			perfMonitor.frameCount++;
			perfMonitor.frameTimeAccumulator += deltaTime;

			// Update performance metrics for min/avg/max calculations and history
			uiManager.updatePerformanceMetrics(perfMonitor, deltaTime);

			// Update performance display every 250ms
			if (currentFrame - perfMonitor.lastPerfUpdate >= perfMonitor.perfUpdateInterval)
			{
				perfMonitor.displayFPS = perfMonitor.frameCount / (currentFrame - perfMonitor.lastPerfUpdate);
				perfMonitor.displayFrameTime = (perfMonitor.frameTimeAccumulator / perfMonitor.frameCount) * 1000.0f; // Convert to ms

				perfMonitor.frameCount = 0;
				perfMonitor.frameTimeAccumulator = 0.0f;
				perfMonitor.lastPerfUpdate = currentFrame;
			}

			// Use the valid dimensions we stored
			int width = lastKnownWidth;
			int height = lastKnownHeight;

			// Final safety check - if we still don't have valid dimensions, skip this frame
			if (width <= 0 || height <= 0)
			{
				glfwPollEvents();
				continue;
			} //// First we do some init stuff
			/// Clear the framebuffer for proper 3D rendering
			/// Tell OpenGL a new frame is about to begin
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			// Set viewport with our validated dimensions
			try
			{
				glViewport(0, 0, width, height);
				checkGLError("glViewport");

				// Clear framebuffer once at the start of the frame
				glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
				checkGLError("glClear");
			}
			catch (...)
			{
				// If OpenGL operations fail, skip this frame
				std::cerr << "OpenGL viewport/clear failed, skipping frame\n";
				glfwPollEvents();
				continue;
			}
			/// Then we handle input
			/// I should probably put this stuff in a separate function instead of having it in the main loop
			// Take care of all GLFW events
			glfwPollEvents();
			input.update();
			if (!ImGui::GetIO().WantCaptureMouse)
			{
				TimerCPU cpuTimer("Input Processing"); // Start CPU timer for input processing
				// Handle camera input
				camera.processInput(input, deltaTime);				// Handle cell selection and dragging
				glm::vec2 mousePos = input.getMousePosition(false); // Get raw screen coordinates
				bool isLeftMousePressed = input.isMouseJustPressed(GLFW_MOUSE_BUTTON_LEFT);
				bool isLeftMouseDown = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
				float scrollDelta = input.getScrollDelta();

				cellManager.handleMouseInput(mousePos, glm::vec2(width, height), camera,
											 isLeftMousePressed, isLeftMouseDown, scrollDelta);
			}
			synthEngine.generateSample();
			//// Then we handle cell simulation
			try
			{
				// Update cell simulation with the delta time
				// GPU timer was moved inside the function because it has multiple elements that need individual timing
				cellManager.updateCells(deltaTime);
				checkGLError("updateCells");
			}
			catch (const std::exception &e)
			{
				std::cerr << "Exception in cell simulation: " << e.what() << "\n";
			}
			catch (...)
			{
				std::cerr << "Unknown exception in cell simulation\n";
			}

			//// Then we handle rendering
			try
			{
				TimerGPU timer("Cell Rendering Update");
				cellManager.renderCells(glm::vec2(width, height), sphereShader, camera);
				checkGLError("renderCells");
			}
			catch (const std::exception &e)
			{
				std::cerr << "Exception in cell rendering: " << e.what() << "\n";
			}
			catch (...)
			{
				std::cerr << "Unknown exception in cell rendering\n";
			} //// Then we handle ImGUI
			// ui.renderUI();
			//  Cell Inspector and Selection UI
			uiManager.renderCellInspector(cellManager);

			// Performance Monitor with readable update rate
			uiManager.renderPerformanceMonitor(cellManager, perfMonitor);

			// Camera Controls
			uiManager.renderCameraControls(cellManager, camera);

			// Genome Editor
			uiManager.renderGenomeEditor();

			// Time Scrubber
			uiManager.renderTimeScrubber(cellManager);

			// Render the demo window if enabled
			if (config::showDemoWindow)
			{
				ImGui::ShowDemoWindow();
			}

			// Renders the ImGUI elements last, so that they are on top of everything else
			try
			{
				ImGui::Render();
				checkGLError("ImGui::Render");

				// Update and Render additional Platform Windows
				if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
				{
					GLFWwindow *backup_current_context = glfwGetCurrentContext();
					ImGui::UpdatePlatformWindows();
					ImGui::RenderPlatformWindowsDefault();
					glfwMakeContextCurrent(backup_current_context);
				}
				ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
				checkGLError("ImGui_ImplOpenGL3_RenderDrawData");
			}
			catch (const std::exception &e)
			{
				std::cerr << "Exception in ImGui: " << e.what() << "\n";
			}
			catch (...)
			{
				std::cerr << "Unknown exception in ImGui\n";
			}

			try
			{
				// Swap the back buffer with the front buffer, so that the rendered image is displayed on the screen
				glfwSwapBuffers(window);
				checkGLError("glfwSwapBuffers");
			}
			catch (const std::exception &e)
			{
				std::cerr << "Exception in buffer swap: " << e.what() << "\n";
				// Try to recover by just swapping buffers
				try
				{
					glfwSwapBuffers(window);
				}
				catch (...)
				{
					// If even buffer swap fails, just continue to next frame
				}
			}
			catch (...)
			{
				std::cerr << "Unknown exception in buffer swap\n";
				// Try to recover by just swapping buffers
				try
				{
					glfwSwapBuffers(window);
				}
				catch (...)
				{
					// If even buffer swap fails, just continue to next frame
				}
			}
		}
	}
	// destroy and terminate everything before ending the ID
	shutdownImGui();
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
