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
#include "scene_manager.h"

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

// Window state management
struct WindowState {
	bool wasMinimized = false;
	bool isCurrentlyMinimized = false;
	int lastKnownWidth = 0;
	int lastKnownHeight = 0;
};

bool handleWindowStateTransitions(GLFWwindow* window, WindowState& state)
{
	int currentWidth, currentHeight;
	glfwGetFramebufferSize(window, &currentWidth, &currentHeight);
	bool isMinimized = (currentWidth == 0 || currentHeight == 0) || glfwGetWindowAttrib(window, GLFW_ICONIFIED);

	// Handle minimize/restore transitions
	if (isMinimized && !state.wasMinimized)
	{
		std::cout << "Window minimized, suspending rendering\n";
		state.wasMinimized = true;
		state.isCurrentlyMinimized = true;
	}
	else if (!isMinimized && state.wasMinimized)
	{
		std::cout << "Window restored, resuming rendering\n";
		state.wasMinimized = false;
		state.isCurrentlyMinimized = false;
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	// If minimized, do minimal processing
	if (state.isCurrentlyMinimized || isMinimized)
	{
		glfwPollEvents();
		glfwSwapBuffers(window);
		std::this_thread::sleep_for(std::chrono::milliseconds(16));
		return true; // Skip frame
	}

	// Store valid dimensions
	if (currentWidth > 0 && currentHeight > 0)
	{
		state.lastKnownWidth = currentWidth;
		state.lastKnownHeight = currentHeight;
	}

	return false; // Don't skip frame
}

// Performance monitoring update
void updatePerformanceMonitoring(PerformanceMonitor& perfMonitor, UIManager& uiManager, float deltaTime, float currentFrame)
{
	perfMonitor.frameCount++;
	perfMonitor.frameTimeAccumulator += deltaTime;
	uiManager.updatePerformanceMetrics(perfMonitor, deltaTime);

	if (currentFrame - perfMonitor.lastPerfUpdate >= perfMonitor.perfUpdateInterval)
	{
		perfMonitor.displayFPS = perfMonitor.frameCount / (currentFrame - perfMonitor.lastPerfUpdate);
		perfMonitor.displayFrameTime = (perfMonitor.frameTimeAccumulator / perfMonitor.frameCount) * 1000.0f;

		perfMonitor.frameCount = 0;
		perfMonitor.frameTimeAccumulator = 0.0f;
		perfMonitor.lastPerfUpdate = currentFrame;
	}
}

// Input processing
void processInput(Input& input, Camera& previewCamera, Camera& mainCamera, CellManager& previewCellManager, CellManager& mainCellManager, 
				  SceneManager& sceneManager, float deltaTime, int width, int height, SynthEngine& synthEngine)
{
	glfwPollEvents();
	input.update();
	
	// Determine which camera and cell manager to use for input based on current scene
	Camera* activeCamera = nullptr;
	CellManager* activeCellManager = nullptr;
	Scene currentScene = sceneManager.getCurrentScene();
	
	if (currentScene == Scene::PreviewSimulation)
	{
		activeCamera = &previewCamera;
		activeCellManager = &previewCellManager;
	}
	else if (currentScene == Scene::MainSimulation)
	{
		activeCamera = &mainCamera;
		activeCellManager = &mainCellManager;
	}
	
	if (!ImGui::GetIO().WantCaptureMouse && activeCamera && activeCellManager)
	{
		TimerCPU cpuTimer("Input Processing");
		activeCamera->processInput(input, deltaTime);
		
		glm::vec2 mousePos = input.getMousePosition(false);
		bool isLeftMousePressed = input.isMouseJustPressed(GLFW_MOUSE_BUTTON_LEFT);
		bool isLeftMouseDown = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
		float scrollDelta = input.getScrollDelta();

		activeCellManager->handleMouseInput(mousePos, glm::vec2(width, height), *activeCamera,
											isLeftMousePressed, isLeftMouseDown, scrollDelta);
	}
	
	synthEngine.generateSample();
}

// Rendering pipeline
void renderFrame(CellManager& previewCellManager, CellManager& mainCellManager, Camera& previewCamera, Camera& mainCamera,
				 UIManager& uiManager, Shader& sphereShader, PerformanceMonitor& perfMonitor, SceneManager& sceneManager, int width, int height)
{
	Scene currentScene = sceneManager.getCurrentScene();
	CellManager* activeCellManager = nullptr;
	Camera* activeCamera = nullptr;
	
	// Select which simulation and camera to render and interact with
	if (currentScene == Scene::PreviewSimulation)
	{
		activeCellManager = &previewCellManager;
		activeCamera = &previewCamera;
	}
	else if (currentScene == Scene::MainSimulation)
	{
		activeCellManager = &mainCellManager;
		activeCamera = &mainCamera;
	}
	
	if (activeCellManager && activeCamera)
	{		// Render the active simulation with its camera
		try
		{
			activeCellManager->renderCells(glm::vec2(width, height), sphereShader, *activeCamera);
			checkGLError("renderCells");
			
			// Render gizmos if enabled
			activeCellManager->renderGizmos(glm::vec2(width, height), *activeCamera, uiManager.showOrientationGizmos);
			checkGLError("renderGizmos");
			
			// Render ring gizmos if enabled
			activeCellManager->renderRingGizmos(glm::vec2(width, height), *activeCamera, uiManager);
			checkGLError("renderRingGizmos");
		}
		catch (const std::exception &e)
		{
			std::cerr << "Exception in cell rendering: " << e.what() << "\n";
		}		// Show the full detailed UI for the active simulation
		uiManager.renderCellInspector(*activeCellManager, sceneManager);
		uiManager.renderPerformanceMonitor(*activeCellManager, perfMonitor, sceneManager);
		uiManager.renderCameraControls(*activeCellManager, *activeCamera, sceneManager);
				// Only show genome editor in Preview Simulation
		if (currentScene == Scene::PreviewSimulation)
		{
			uiManager.renderGenomeEditor(previewCellManager, sceneManager);
		}
		
		// Only show time scrubber in Preview Simulation
		if (currentScene == Scene::PreviewSimulation)
		{
			uiManager.renderTimeScrubber(*activeCellManager, sceneManager);
		}
		
		uiManager.renderSceneSwitcher(sceneManager, previewCellManager, mainCellManager);
	}

	if (config::showDemoWindow)
	{
		ImGui::ShowDemoWindow();
	}
}

void updateSimulation(CellManager& previewCellManager, CellManager& mainCellManager, SceneManager& sceneManager)
{
	// Only update simulations if not paused
	if (sceneManager.isPaused())
	{
		return;
	}
	// Update only the active scene simulation
	float timeStep = config::physicsTimeStep;// *sceneManager.getSimulationSpeed();
	Scene currentScene = sceneManager.getCurrentScene();
	
	try
	{
		if (currentScene == Scene::PreviewSimulation)
		{
			// Update only Preview Simulation
			previewCellManager.updateCells(timeStep);
			checkGLError("updateCells - preview");
			
			// Update preview simulation time tracking
			sceneManager.updatePreviewSimulationTime(timeStep);
		}
		else if (currentScene == Scene::MainSimulation)
		{
			// Update only Main Simulation
			mainCellManager.updateCells(timeStep);
			checkGLError("updateCells - main");
		}
	}
	catch (const std::exception& e)
	{
		std::cerr << "Exception in simulation: " << e.what() << "\n";
	}
	catch (...)
	{
		std::cerr << "Unknown exception in simulation\n";
	}
}

// ImGui rendering
void renderImGui(const ImGuiIO& io)
{
	try
	{
		ImGui::Render();
		checkGLError("ImGui::Render");

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
		Input input;		input.init(window);		// Initialise the cameras - separate camera for each scene
		Camera previewCamera(glm::vec3(0.0f, 0.0f, 75.0f)); // Start further back to see more cells
		Camera mainCamera(glm::vec3(0.0f, 0.0f, 75.0f)); // Start at same position for consistent view
		// Initialise the UI manager // We dont have any ui to manage yet
		ToolState toolState;
		UIManager uiManager;		// Initialise cells - create separate cell managers for each scene
		CellManager previewCellManager;
		CellManager mainCellManager;
		
		// Initialize Preview Simulation
		previewCellManager.addGenomeToBuffer(uiManager.currentGenome);
		previewCellManager.addCellToStagingBuffer(ComputeCell()); // spawns 1 cell at 0,0,0
		previewCellManager.addStagedCellsToGPUBuffer(); // Force immediate GPU buffer sync
		
		// Initialize Main Simulation
		mainCellManager.addGenomeToBuffer(uiManager.currentGenome);
		mainCellManager.addCellToStagingBuffer(ComputeCell()); // spawns 1 cell at 0,0,0
		mainCellManager.addStagedCellsToGPUBuffer(); // Force immediate GPU buffer sync
		
		// Ensure both simulations have proper initial state by running one update cycle
		previewCellManager.updateCells(config::physicsTimeStep);
		mainCellManager.updateCells(config::physicsTimeStep);

	AudioEngine audioEngine;
	audioEngine.init();
	audioEngine.start();
	SynthEngine synthEngine;

	// Timing variables
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;
	float accumulator = 0.0f;

	// Performance monitoring struct
	PerformanceMonitor perfMonitor{};

	// Scene management
	SceneManager sceneManager;

	// Window state tracking
	WindowState windowState;

	// Main while loop
	while (!glfwWindowShouldClose(window))
	{
		//cellManager.spawnCells(1); // spawns 1 cell somewhere, for debugging

		// Calculate delta time
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		deltaTime = std::clamp(deltaTime, 0.0f, config::maxDeltaTime);
		accumulator += deltaTime;
		accumulator = std::clamp(accumulator, 0.0f, config::maxAccumulatorTime);
		float tickPeriod = config::physicsTimeStep / sceneManager.getSimulationSpeed();//config::physicsSpeed;

		// Check window state first - before any OpenGL operations
		if (handleWindowStateTransitions(window, windowState))
		{
			// If the window state handling function indicates to skip the frame, continue to the next iteration
			continue;
		}
		// Update performance metrics for min/avg/max calculations and history
		updatePerformanceMonitoring(perfMonitor, uiManager, deltaTime, currentFrame);

		// Use the valid dimensions we stored
		int width = windowState.lastKnownWidth;
		int height = windowState.lastKnownHeight;

		// Final safety check - if we still don't have valid dimensions, skip this frame
		if (width <= 0 || height <= 0)
		{
			glfwPollEvents();
			continue;
		}
		/// First we do some init stuff
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
		// I should probably put this stuff in a separate function instead of having it in the main loop
		// Take care of all GLFW events
		processInput(input, previewCamera, mainCamera, previewCellManager, mainCellManager, sceneManager, deltaTime, width, height, synthEngine);

		/// Then we handle cell simulation
		while (accumulator >= tickPeriod)
		{
			updateSimulation(previewCellManager, mainCellManager, sceneManager);
			accumulator -= tickPeriod;
		}
		/// Then we handle rendering
		renderFrame(previewCellManager, mainCellManager, previewCamera, mainCamera, uiManager, sphereShader, perfMonitor, sceneManager, width, height);

		// Update all the timers
		TimerManager::instance().finalizeFrame();
		TimerManager::instance().drawImGui();
		// ImGui rendering
		renderImGui(io);

		try
		{
			// Swap the back buffer with the front buffer, so that the rendered image is displayed on the screen
			glfwSwapBuffers(window);
			checkGLError("glfwSwapBuffers");
		}
		catch (const std::exception &e)
		{
			std::cerr << "Exception in framebuffer swap: " << e.what() << "\n";
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
			std::cerr << "Unknown exception in framebuffer swap\n";
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
