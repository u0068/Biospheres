#pragma once

// Third-party includes
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// Standard includes
#include <memory>
#include <string>

// Forward declarations
class BakerUI;
class PreviewRenderer;
class MeshBaker;
class CameraController;

/**
 * Main application class for the Metaball Baker Tool
 * Manages window lifecycle, OpenGL context, and coordinates all subsystems
 */
class BakerApplication {
public:
    BakerApplication();
    ~BakerApplication();
    
    // Application lifecycle
    bool initialize();
    void run();
    void shutdown();
    
    // Window management
    GLFWwindow* getWindow() const { return window; }
    glm::vec2 getWindowSize() const;
    
private:
    // Core systems
    GLFWwindow* window;
    std::unique_ptr<BakerUI> ui;
    std::unique_ptr<PreviewRenderer> previewRenderer;
    std::unique_ptr<MeshBaker> meshBaker;
    std::unique_ptr<CameraController> camera;
    
    // Application state
    bool initialized;
    float deltaTime;
    float lastFrame;
    
    // Window properties
    static constexpr int WINDOW_WIDTH = 1280;
    static constexpr int WINDOW_HEIGHT = 720;
    static constexpr const char* WINDOW_TITLE = "Metaball Baker Tool";
    
    // Private methods
    bool initializeGLFW();
    bool createWindow();
    bool initializeGLAD();
    bool initializeImGui();
    void setupOpenGLState();
    
    void processInput();
    void update();
    void render();
    
    // Callbacks
    static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
    static void errorCallback(int error, const char* description);
    
    // Utility
    void calculateDeltaTime();
    bool shouldClose() const;
};