#pragma once
#include<GLFW/glfw3.h>
#include<iostream>

void initGLFW();

GLFWwindow* createWindow();

void framebuffer_size_callback(GLFWwindow* window, int width, int height);

//void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity,
//    GLsizei length, const char* message, const void* userParam);