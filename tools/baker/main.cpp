// Metaball Baker Tool - Main Entry Point
// Standalone application for baking metaball geometry variations

// Third-party includes
#define WIN32_LEAN_AND_MEAN
#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/imgui_impl_glfw.h"
#include "../../third_party/imgui/imgui_impl_opengl3.h"

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <chrono>
#include <thread>

// Baker tool includes
#include "baker_application.h"

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
    try
    {
        // Initialize baker application
        BakerApplication app;
        
        if (!app.initialize())
        {
            std::cerr << "Failed to initialize Baker Application\n";
            return EXIT_FAILURE;
        }
        
        std::cout << "Metaball Baker Tool initialized successfully\n";
        
        // Run main application loop
        app.run();
        
        // Cleanup
        app.shutdown();
        
        std::cout << "Baker Tool shutdown complete\n";
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Baker Tool exception: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception in Baker Tool\n";
        return EXIT_FAILURE;
    }
}