#pragma once
#include <GLFW/glfw3.h>
#include <imgui.h>
#include "imgui_impl_opengl3.h"
#include "imgui_impl_glfw.h"


ImGuiIO& initImGui(GLFWwindow* window);

void shutdownImGui();
