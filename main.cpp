// Third-party includes
#define WIN32_LEAN_AND_MEAN
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_glfw.h"
#include "third_party/imgui/imgui_impl_opengl3.h"

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <algorithm>
#include <thread>
#include <chrono>

// Core includes
#include "src/core/config.h"

// Audio includes
#define MINIAUDIO_IMPLEMENTATION
#include "src/audio/audio_engine.h"
#include "src/audio/synthesizer.h"

// Simulation includes
#include "src/simulation/cell/cell_manager.h"
#include "src/simulation/spatial/spatial_grid_system.h"
#include "src/simulation/cpu_preview/cpu_preview_system.h"
#include "src/simulation/cpu_preview/cpu_genome_converter.h"

// Rendering includes
#include "src/rendering/core/shader_class.h"
#include "src/rendering/core/glad_helpers.h"
#include "src/rendering/core/glfw_helpers.h"
#include "src/rendering/camera/camera.h"
#include "src/rendering/systems/brush_renderer.h"
#include "src/rendering/systems/visualization_renderer.h"

// UI includes
#include "src/ui/ui_manager.h"
#include "src/ui/imgui_helpers.h"

// Input includes
#include "src/input/input.h"
#include "src/input/injection_system.h"

// Utility includes
#include "src/utils/timer.h"

// Scene includes
#include "src/scene/scene_manager.h"

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
				  SceneManager& sceneManager, float deltaTime, int width, int height, SynthEngine& synthEngine, 
				  InjectionSystem& injectionSystem, SpatialGridSystem& spatialGrid)
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
		// For Preview Simulation, use main cell manager for input infrastructure only
		// The CPU Preview System handles the actual simulation
		activeCellManager = &mainCellManager;
	}
	else if (currentScene == Scene::MainSimulation)
	{
		activeCamera = &mainCamera;
		activeCellManager = &mainCellManager;
	}
	
	// Handle keyboard input for injection mode switching (always process, even if ImGui wants keyboard)
	// Use static variables to track key state for "just pressed" behavior
	static bool key1WasPressed = false;
	static bool key2WasPressed = false;
	static bool key3WasPressed = false;
	
	bool key1IsPressed = input.isKeyPressed(GLFW_KEY_1);
	bool key2IsPressed = input.isKeyPressed(GLFW_KEY_2);
	bool key3IsPressed = input.isKeyPressed(GLFW_KEY_3);
	
	if (key1IsPressed && !key1WasPressed) {
		injectionSystem.handleKeyInput(GLFW_KEY_1);
	}
	if (key2IsPressed && !key2WasPressed) {
		injectionSystem.handleKeyInput(GLFW_KEY_2);
	}
	if (key3IsPressed && !key3WasPressed) {
		injectionSystem.handleKeyInput(GLFW_KEY_3);
	}
	
	key1WasPressed = key1IsPressed;
	key2WasPressed = key2IsPressed;
	key3WasPressed = key3IsPressed;
	
	if (!ImGui::GetIO().WantCaptureMouse && activeCamera && activeCellManager)
	{
		TimerCPU cpuTimer("Input Processing");
		
		// Handle injection system input first
		glm::vec2 mousePos = input.getMousePosition(false);
		bool isLeftMousePressed = input.isMouseJustPressed(GLFW_MOUSE_BUTTON_LEFT);
		bool isLeftMouseDown = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
		float scrollDelta = input.getScrollDelta();
		
		// Update brush position continuously with mouse movement
		injectionSystem.handleMouseMove(mousePos, *activeCamera, glm::vec2(width, height));
		
		// Handle mouse clicks for injection
		if (isLeftMousePressed) {
			injectionSystem.handleMouseClick(mousePos, *activeCamera, spatialGrid, glm::vec2(width, height));
		}
		if (isLeftMouseDown) {
			injectionSystem.handleMouseDrag(mousePos, *activeCamera, spatialGrid, glm::vec2(width, height));
		}
		
		// Handle scroll wheel - injection system will route appropriately based on mode
		if (input.hasScrollInput()) {
			InjectionMode currentMode = injectionSystem.getCurrentMode();
			if (currentMode == InjectionMode::Density || currentMode == InjectionMode::Velocity) {
				// Route to injection system for plane distance adjustment
				injectionSystem.handleScrollWheel(scrollDelta);
			} else {
				// Route to camera for zoom (existing behavior)
				activeCamera->processMouseScroll(scrollDelta);
			}
		}
		
		// Process camera input (but skip scroll if handled by injection system)
		if (injectionSystem.getCurrentMode() == InjectionMode::CellSelection) {
			activeCamera->processInput(input, deltaTime);
		} else {
			// Process camera input without scroll wheel
			// We need to create a modified version that doesn't handle scroll
			// For now, just process normally - the scroll handling above takes precedence
			activeCamera->processInput(input, deltaTime);
		}
		
		// Handle cell manager input only in cell selection mode
		if (injectionSystem.getCurrentMode() == InjectionMode::CellSelection) {
			activeCellManager->handleMouseInput(mousePos, glm::vec2(width, height), *activeCamera,
												isLeftMousePressed, isLeftMouseDown, scrollDelta);
		}
	}
	
	synthEngine.generateSample();
}

// Rendering pipeline
void renderFrame(CellManager& previewCellManager, CellManager& mainCellManager, Camera& previewCamera, Camera& mainCamera,
				 UIManager& uiManager, Shader& sphereShader, PerformanceMonitor& perfMonitor, SceneManager& sceneManager, int width, int height,
				 InjectionSystem& injectionSystem, BrushRenderer& brushRenderer, SpatialGridSystem& spatialGrid, VisualizationRenderer& visualizationRenderer,
				 CPUPreviewSystem& cpuPreviewSystem)
{
	Scene currentScene = sceneManager.getCurrentScene();
	CellManager* activeCellManager = nullptr;
	Camera* activeCamera = nullptr;
	
	// Debug: Print current scene (only once per scene change)
	static Scene lastScene = Scene::MainSimulation;
	if (currentScene != lastScene) {
		std::cout << "Scene changed to: " << (currentScene == Scene::PreviewSimulation ? "PreviewSimulation" : "MainSimulation") << std::endl;
		lastScene = currentScene;
	}
	
	// Select which simulation and camera to render and interact with
	if (currentScene == Scene::PreviewSimulation)
	{
		// Use CPU Preview System exclusively for Preview Simulation
		activeCamera = &previewCamera;
		
		// Upload CPU Preview System visual data to instance buffer for rendering
		try {
			// Ensure visual data is current before rendering (needed when paused)
			cpuPreviewSystem.ensureVisualDataCurrent();
			cpuPreviewSystem.uploadVisualDataToGPU();
		}
		catch (const std::exception& e) {
			std::cerr << "CPU Preview System GPU upload error: " << e.what() << std::endl;
		}
		
		// Don't use activeCellManager for Preview Simulation - CPU system handles everything
		activeCellManager = nullptr;
	}
	else if (currentScene == Scene::MainSimulation)
	{
		activeCellManager = &mainCellManager;
		activeCamera = &mainCamera;
	}
	
	if (activeCamera)
	{
		// Render the active simulation with its camera
		try
		{
			if (currentScene == Scene::PreviewSimulation)
			{
				// Render CPU Preview System data using direct sphere mesh rendering
				// The CPU Preview System has already uploaded data to the instance buffer
				const auto* visualData = cpuPreviewSystem.getVisualData();
				

				
				if (visualData && visualData->activeCount > 0)
				{
					// Get the sphere mesh from preview cell manager for rendering infrastructure
					auto& sphereMesh = previewCellManager.getSphereMesh();
					
					// Set up the sphere mesh to use the CPU Preview System's instance buffer (only once)
					static bool instanceBufferSetup = false;
					if (!instanceBufferSetup) {
						GLuint cpuInstanceBuffer = cpuPreviewSystem.getTripleBufferSystem()->getInstanceBuffer();
						sphereMesh.setupInstanceBuffer(cpuInstanceBuffer);
						instanceBufferSetup = true;
						std::cout << "CPU Preview System: Instance buffer setup completed\n";
					}
					
					// Render using the sphere shader
					sphereShader.use();
					
					// Temporarily disable face culling for preview cells to debug fragmentation
					glDisable(GL_CULL_FACE);
					
					// Bind a white texture (create once, reuse)
					static GLuint whiteTexture = 0;
					if (whiteTexture == 0) {
						glGenTextures(1, &whiteTexture);
						glBindTexture(GL_TEXTURE_2D, whiteTexture);
						unsigned char whitePixel[4] = {255, 255, 255, 255}; // White RGBA
						glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
						glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
						std::cout << "Created white texture for CPU Preview System\n";
					}
					glActiveTexture(GL_TEXTURE0);
					glBindTexture(GL_TEXTURE_2D, whiteTexture);
					sphereShader.setInt("uTexture", 0);
					
					// Set up camera matrices
					glm::mat4 view = activeCamera->getViewMatrix();
					glm::mat4 projection = glm::perspective(glm::radians(45.0f), 
														   static_cast<float>(width) / static_cast<float>(height), 
														   0.1f, 1000.0f);
					
					// Set uniforms with correct names (matching sphere shader expectations)
					sphereShader.setMat4("uProjection", projection);
					sphereShader.setMat4("uView", view);
					sphereShader.setVec3("uCameraPos", activeCamera->getPosition());
					sphereShader.setVec3("uLightDir", glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f)));
					sphereShader.setFloat("uTime", static_cast<float>(glfwGetTime()));
					
					// Set selection highlighting (no selection in CPU Preview System)
					sphereShader.setVec3("uSelectedCellPos", glm::vec3(-9999.0f)); // Invalid position
					sphereShader.setFloat("uSelectedCellRadius", 0.0f);
					
					// Render the cells
					sphereMesh.render(visualData->activeCount); // Render all active cells
					
					// Re-enable face culling for other rendering
					glEnable(GL_CULL_FACE);
				}
			}
			else if (activeCellManager)
			{
				// Render GPU-based simulation (Main Simulation)
				activeCellManager->renderCells(glm::vec2(width, height), sphereShader, *activeCamera, uiManager.wireframeMode);
			}
			checkGLError("renderCells");
			
			// Render additional elements only for GPU-based simulations
			if (activeCellManager)
			{
				// Render particles
				activeCellManager->renderParticles(*activeCamera, glm::vec2(width, height));
				checkGLError("renderParticles");
				
				// Render gizmos if enabled
				activeCellManager->renderGizmos(glm::vec2(width, height), *activeCamera, uiManager.showOrientationGizmos);
				checkGLError("renderGizmos");
				
				// Render ring gizmos if enabled
				activeCellManager->renderRingGizmos(glm::vec2(width, height), *activeCamera, uiManager);
				checkGLError("renderRingGizmos");
				activeCellManager->renderAnchorGizmos(glm::vec2(width, height), *activeCamera, uiManager);
				checkGLError("renderAnchorGizmos");
				
				// Render adhesionSettings lines if enabled
				activeCellManager->renderAdhesionLines(glm::vec2(width, height), *activeCamera, uiManager.showAdhesionLines);
				checkGLError("renderAdhesionLines");
			}
			else if (currentScene == Scene::PreviewSimulation)
			{
				// CPU Preview System: Render gizmos using GPU instanced rendering
				if (uiManager.showOrientationGizmos) {
					cpuPreviewSystem.renderGizmos(glm::vec2(width, height), *activeCamera, uiManager.showOrientationGizmos);
					checkGLError("CPU Preview renderGizmos");
					
					// Render ring gizmos (split direction indicators)
					cpuPreviewSystem.renderRingGizmos(glm::vec2(width, height), *activeCamera, uiManager);
					checkGLError("CPU Preview renderRingGizmos");
					
					// Render anchor gizmos (adhesion connection indicators)
					cpuPreviewSystem.renderAnchorGizmos(glm::vec2(width, height), *activeCamera, uiManager);
					checkGLError("CPU Preview renderAnchorGizmos");
				}
				
				// Note: Adhesion lines not yet implemented for CPU Preview System
			}
			
			// Render injection brush if visible
			if (injectionSystem.isBrushVisible()) {
				brushRenderer.renderBrush(injectionSystem.getBrushPosition(), 
										 injectionSystem.getInjectionRadius(),
										 injectionSystem.getCurrentMode(),
										 *activeCamera, glm::vec2(width, height),
										 injectionSystem.isCurrentlyInjecting());
				checkGLError("renderBrush");
			}
			
			// Render fluid visualization
			glm::mat4 viewMatrix = activeCamera->getViewMatrix();
			glm::mat4 projectionMatrix = glm::perspective(glm::radians(45.0f), 
														 static_cast<float>(width) / static_cast<float>(height), 
														 0.1f, 1000.0f);
			visualizationRenderer.render(spatialGrid, viewMatrix, projectionMatrix);
			checkGLError("renderVisualization");
		}
		catch (const std::exception &e)
		{
			std::cerr << "Exception in cell rendering: " << e.what() << "\n";
		}
		// Show the full detailed UI for the active simulation
		if (currentScene == Scene::PreviewSimulation)
		{
			// For Preview Simulation, use main cell manager for UI infrastructure
			// TODO: Modify these UI components to work with CPU Preview System
			uiManager.renderCellInspector(mainCellManager, sceneManager);
			uiManager.renderPerformanceMonitor(mainCellManager, perfMonitor, sceneManager, &cpuPreviewSystem);
			uiManager.renderCameraControls(mainCellManager, *activeCamera, sceneManager);
		}
		else
		{
			// For Main Simulation, use the active cell manager normally
			uiManager.renderCellInspector(*activeCellManager, sceneManager);
			uiManager.renderPerformanceMonitor(*activeCellManager, perfMonitor, sceneManager, nullptr);
			uiManager.renderCameraControls(*activeCellManager, *activeCamera, sceneManager);
		}
		
		// Show injection controls
		uiManager.renderInjectionControls(injectionSystem, spatialGrid, visualizationRenderer);

		// Only show genome editor in Preview Simulation
		if (currentScene == Scene::PreviewSimulation)
		{
			// Connect genome editor to CPU Preview System for instant parameter updates
			uiManager.renderGenomeEditor(mainCellManager, sceneManager, &cpuPreviewSystem);
		}
		
		// Only show time scrubber in Preview Simulation
		if (currentScene == Scene::PreviewSimulation)
		{
			// Time scrubber now works with CPU Preview System
			uiManager.renderTimeScrubber(mainCellManager, sceneManager, &cpuPreviewSystem);
		}
		
		uiManager.renderSceneSwitcher(sceneManager, previewCellManager, mainCellManager);
	}

	if (config::showDemoWindow)
	{
		ImGui::ShowDemoWindow();
	}
}

void updateSimulation(CellManager& previewCellManager, CellManager& mainCellManager, SceneManager& sceneManager, CPUPreviewSystem& cpuPreviewSystem)
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
		if (currentScene == Scene::PreviewSimulation && sceneManager.isPreviewSystemActive())
		{
			// Use CPU Preview System exclusively for Preview Simulation (native SoA)
			try {
				cpuPreviewSystem.update(timeStep);
			}
			catch (const std::exception& e) {
				std::cerr << "CPU Preview System update error: " << e.what() << std::endl;
			}
			
			// Update preview simulation time tracking
			sceneManager.updatePreviewSimulationTime(timeStep);
		}
		else if (currentScene == Scene::MainSimulation && sceneManager.isMainSystemActive())
		{
			// Update only Main Simulation (native AoS)
			mainCellManager.updateCells(timeStep);
			checkGLError("updateCells - main");
			
			// Update particles
			mainCellManager.updateParticles(timeStep);
			checkGLError("updateParticles - main");
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
	// Load the sphere shader for instanced rendering (Phagocyte)
    Shader sphereShader("shaders/rendering/cell_types/phagocyte/sphere.vert", 
                        "shaders/rendering/cell_types/phagocyte/sphere.frag");

	// Initialize ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 330");
	Input input;		input.init(window);		// Initialise the cameras - separate camera for each scene
	Camera previewCamera(glm::vec3(0.0f, 0.0f, 75.0f)); // Start further back to see more cells
	Camera mainCamera(glm::vec3(0.0f, 0.0f, 75.0f)); // Start at same position for consistent view
	// Initialise the UI manager // We dont have any ui to manage yet
	ToolState toolState;
	UIManager uiManager;		// Initialise cells - create separate cell managers for each scene
	CellManager previewCellManager;
	CellManager mainCellManager;
	
	// Scene management
	SceneManager sceneManager;
	
	// Initialize CPU Preview System for high-performance genome iteration
	CPUPreviewSystem cpuPreviewSystem;
	
	// Mark preview simulation (used only for rendering infrastructure, not simulation)
	previewCellManager.isPreviewSimulation = true;
	
	// Initialize scene management with independent systems (Requirements 3.4, 3.5)
	sceneManager.coordinateWithCPUPreviewSystem(&cpuPreviewSystem);
	sceneManager.coordinateWithMainCellManager(&mainCellManager);
	sceneManager.switchToPreviewMode(); // Start in Preview mode by default
	sceneManager.setPreviewSystemActive(true); // Ensure preview system is active
	sceneManager.setMainSystemActive(true); // Main system available but not active
	
	// Initialize CPU Preview System with initial cell data
	// The CPU Preview System will handle all preview simulation logic

	// Initialize Main Simulation
	mainCellManager.addGenomeToBuffer(uiManager.currentGenome);
	ComputeCell mainCell{};
	mainCell.orientation = glm::vec4(uiManager.currentGenome.initialOrientation.x, uiManager.currentGenome.initialOrientation.y, uiManager.currentGenome.initialOrientation.z, uiManager.currentGenome.initialOrientation.w);
	mainCell.genomeOrientation = mainCell.orientation;
	mainCellManager.addCellToStagingBuffer(mainCell); // spawns 1 cell at 0,0,0
	mainCellManager.addStagedCellsToQueueBuffer(); // Force immediate GPU buffer sync
	
	// Ensure main simulation has proper initial state by running one update cycle
	mainCellManager.updateCells(config::physicsTimeStep);
	
	// Initialize CPU Preview System FIRST
	try {
		cpuPreviewSystem.initialize();
		std::cout << "CPU Preview System initialized successfully\n";
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to initialize CPU Preview System: " << e.what() << std::endl;
	}
	
	// Connect CPU Preview System to existing instance buffer for seamless rendering integration
	try {
		GLuint previewInstanceBuffer = previewCellManager.getInstanceBuffer();
		if (previewInstanceBuffer != 0) {
			cpuPreviewSystem.getTripleBufferSystem()->setInstanceBuffer(previewInstanceBuffer);
			std::cout << "CPU Preview System connected to existing instance buffer (ID: " << previewInstanceBuffer << ")\n";
		} else {
			std::cerr << "Warning: Preview CellManager instance buffer not available for CPU Preview System\n";
		}
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to connect CPU Preview System to instance buffer: " << e.what() << std::endl;
	}
	
	// Add default cell for genome editing AFTER initialization
	try {
		CPUCellParameters defaultCell;
		defaultCell.position = glm::vec3(0.0f, 0.0f, 0.0f);
		defaultCell.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
		defaultCell.orientation = glm::quat(
			uiManager.currentGenome.initialOrientation.w,
			uiManager.currentGenome.initialOrientation.x,
			uiManager.currentGenome.initialOrientation.y,
			uiManager.currentGenome.initialOrientation.z
		);
		defaultCell.mass = 1.0f;
		defaultCell.radius = 1.0f;
		defaultCell.cellType = 0;
		defaultCell.genomeID = 0;
		
		// Convert current genome to CPU format and apply to default cell
		defaultCell.genome = CPUGenomeConverter::convertToCPUFormat(uiManager.currentGenome);
		
		cpuPreviewSystem.addCell(defaultCell);
		
		// Also apply genome parameters after cell creation to ensure they're applied
		CPUGenomeConverter::applyGenomeToPreviewSystem(cpuPreviewSystem, uiManager.currentGenome);
		
		// Force immediate visual data update after adding cell
		cpuPreviewSystem.triggerInstantResimulation();
		
		std::cout << "CPU Preview System initialized with default cell\n";
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to add default cell to CPU Preview System: " << e.what() << std::endl;
	}

	AudioEngine audioEngine;
	audioEngine.init();
	audioEngine.start();
	SynthEngine synthEngine;

	// Initialize spatial grid system and injection system
	SpatialGridSystem spatialGrid;
	// Re-enable spatial grid initialization to test
	spatialGrid.initialize();
	// Re-enable injection system to test
	InjectionSystem injectionSystem;
	
	// Initialize brush renderer
	BrushRenderer brushRenderer;
	brushRenderer.initialize();
	
	// Initialize visualization renderer
	VisualizationRenderer visualizationRenderer;
	visualizationRenderer.initialize(config::GRID_RESOLUTION, config::WORLD_SIZE, glm::vec3(0.0f));
	


	// Timing variables
	float deltaTime = 0.0f;
	float lastFrame = 0.0f;
	float accumulator = 0.0f;

	// Performance monitoring struct
	PerformanceMonitor perfMonitor{};

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
		processInput(input, previewCamera, mainCamera, previewCellManager, mainCellManager, sceneManager, deltaTime, width, height, synthEngine, injectionSystem, spatialGrid);

		/// Handle debounced genome resimulation for preview simulation
		if (sceneManager.getCurrentScene() == Scene::PreviewSimulation)
		{
			uiManager.updateDebouncedGenomeResimulation(previewCellManager, sceneManager, deltaTime);
		}

		/// Then we handle cell simulation
		while (accumulator >= tickPeriod)
		{
			updateSimulation(previewCellManager, mainCellManager, sceneManager, cpuPreviewSystem);
			accumulator -= tickPeriod;
		}
		/// Then we handle rendering
		renderFrame(previewCellManager, mainCellManager, previewCamera, mainCamera, uiManager, sphereShader, perfMonitor, sceneManager, width, height, injectionSystem, brushRenderer, spatialGrid, visualizationRenderer, cpuPreviewSystem);

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
	
	// Cleanup spatial grid system
	spatialGrid.cleanup();
	
	// Cleanup brush renderer
	brushRenderer.cleanup();
	
	// Cleanup visualization renderer
	visualizationRenderer.cleanup();
	}
	// Shutdown ImGui
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}
