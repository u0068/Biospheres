#include "imgui_helpers.h"
#include "../core/config.h"

ImGuiIO& initImGui(GLFWwindow* window)
{
	// Initialize ImGUI
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// ENABLE docking
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	
	// Disable viewports to keep windows contained within main window
	// If you want windows to go outside, uncomment the line below:
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
	
	// Keep windows within the main viewport
	io.ConfigViewportsNoAutoMerge = false;
	io.ConfigViewportsNoTaskBarIcon = true;

	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(config::GLSL_VERSION);

	// [optional] Force font atlas generation
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	return io;
}

void shutdownImGui()
{
	// Deletes all ImGUI instances
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}