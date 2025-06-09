#include"imgui.h"
#include"imgui_impl_glfw.h"
#include"imgui_impl_opengl3.h"
#include"shader_class.h"

#include<iostream>
#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include<glm.hpp>

#include "cell_manager.h"
#include "glad_helpers.h"
#include "glfw_helpers.h"
#include "imgui_helpers.h"
#include "config.h"
#include "fullscreen_quad.h"
#include "input.h"
#include "ui_manager.h"

//// If you're a dev, please help me I have no idea what im doing

int main()
{
	initGLFW();
	GLFWwindow* window = createWindow();
	initGLAD(window);

	// This is the quad that we will render everything on
	initFullscreenQuad();
	// Load the cell shader
	Shader cellShader("shaders/default.vert", "shaders/cells.frag");

	const ImGuiIO& io = initImGui(window); // This also initialises ImGui io

	Input input;
	input.init(window);

	// Initialise the UI manager // We dont have any ui to manage yet
	UIManager ui;
	ToolState toolState;

	// Initialise cells
	CellManager cellManager;
	//for (int i = 0; i < 1000; i++)
	//{
	//	// Add a cell with random position, velocity, mass and radius
	//	cellManager.addCell(
	//		glm::vec3(rand() % 100 - 50, rand() % 100 - 50, rand() % 100 - 50), // Random position
	//		glm::vec3(rand() % 10 - 5, rand() % 10 - 5, rand() % 10 - 5), // Random velocity
	//		static_cast<float>(rand() % 10 + 1), // Random mass between 1 and 10
	//		static_cast<float>(rand() % 5 + 1) // Random radius between 1 and 5
	//	);
	//}
	cellManager.addCell(
		glm::vec3(0.0f), // Position
		glm::vec3(0.0), // Velocity
		1.0f, // Mass
		1.0f // Radius
	);

	// Main while loop
	while (!glfwWindowShouldClose(window))
	{
		//// First we do some init stuff
		/// Don't need to clear the framebuffer, as all pixels will be overwritten by the fullscreen quad
		/// Tell OpenGL a new frame is about to begin
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		//// Then we handle input
		/// I should probably put this stuff in a separate function instead of having it in the main loop
		// Take care of all GLFW events
		glfwPollEvents();
		input.update();

		if (!ImGui::GetIO().WantCaptureMouse)
		{
			// Handle input that affects the simulation
			//input.handleInput();
		}


		//// Then we handle cell simulation
		cellManager.updateCells();



		//// Then we handle rendering
		cellManager.renderCells(glm::vec2(width, height), cellShader);

		//// Then we handle ImGUI
		//ui.renderUI();
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		ImGui::Text("Frame time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);

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
