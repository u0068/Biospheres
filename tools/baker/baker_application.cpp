#include "baker_application.h"

// Third-party includes
#include "../../third_party/imgui/imgui.h"
#include "../../third_party/imgui/imgui_impl_glfw.h"
#include "../../third_party/imgui/imgui_impl_opengl3.h"

// Baker tool includes
#include "baker_ui.h"
#include "preview_renderer.h"
#include "mesh_baker.h"
#include "camera_controller.h"

// Standard includes
#include <iostream>
#include <stdexcept>
#include <filesystem>

BakerApplication::BakerApplication()
    : window(nullptr)
    , initialized(false)
    , deltaTime(0.0f)
    , lastFrame(0.0f)
{
}

BakerApplication::~BakerApplication()
{
    if (initialized)
    {
        shutdown();
    }
}

bool BakerApplication::initialize()
{
    if (initialized)
    {
        return true;
    }
    
    try
    {
        // Set working directory to project root
        // Executable is now in bin/baker/Platform/Configuration/, so go up 4 levels to reach project root
        std::filesystem::path exePath = std::filesystem::current_path();
        std::filesystem::path projectRoot = exePath.parent_path().parent_path().parent_path().parent_path();
        
        // Check if we can find the assets directory to confirm we're in the right place
        if (std::filesystem::exists(projectRoot / "assets"))
        {
            std::filesystem::current_path(projectRoot);
            std::cout << "Set working directory to: " << std::filesystem::current_path() << "\n";
        }
        else
        {
            std::cout << "Warning: Could not find project root, using current directory: " << std::filesystem::current_path() << "\n";
        }
        // Initialize GLFW
        if (!initializeGLFW())
        {
            std::cerr << "Failed to initialize GLFW\n";
            return false;
        }
        
        // Create window
        if (!createWindow())
        {
            std::cerr << "Failed to create window\n";
            return false;
        }
        
        // Initialize GLAD
        if (!initializeGLAD())
        {
            std::cerr << "Failed to initialize GLAD\n";
            return false;
        }
        
        // Setup OpenGL state
        setupOpenGLState();
        
        // Initialize ImGui
        if (!initializeImGui())
        {
            std::cerr << "Failed to initialize ImGui\n";
            return false;
        }
        
        // Initialize subsystems
        ui = std::make_unique<BakerUI>();
        previewRenderer = std::make_unique<PreviewRenderer>();
        meshBaker = std::make_unique<MeshBaker>();
        camera = std::make_unique<CameraController>();
        
        // Initialize subsystems
        if (!previewRenderer->initialize())
        {
            std::cerr << "Failed to initialize preview renderer\n";
            return false;
        }
        
        if (!camera->initialize())
        {
            std::cerr << "Failed to initialize camera controller\n";
            return false;
        }
        
        initialized = true;
        std::cout << "Baker Application initialized successfully\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during initialization: " << e.what() << "\n";
        return false;
    }
}

void BakerApplication::run()
{
    if (!initialized)
    {
        throw std::runtime_error("Application not initialized");
    }
    
    std::cout << "Starting Baker Tool main loop\n";
    
    // Main application loop
    while (!shouldClose())
    {
        // Calculate delta time
        calculateDeltaTime();
        
        // Process input
        processInput();
        
        // Update application state
        update();
        
        // Render frame
        render();
        
        // Swap buffers and poll events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    
    std::cout << "Baker Tool main loop ended\n";
}

void BakerApplication::shutdown()
{
    if (!initialized)
    {
        return;
    }
    
    std::cout << "Shutting down Baker Application\n";
    
    // Cleanup subsystems
    camera.reset();
    meshBaker.reset();
    previewRenderer.reset();
    ui.reset();
    
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Cleanup GLFW
    if (window)
    {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
    
    initialized = false;
}

glm::vec2 BakerApplication::getWindowSize() const
{
    if (!window)
    {
        return glm::vec2(0.0f);
    }
    
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    return glm::vec2(static_cast<float>(width), static_cast<float>(height));
}

bool BakerApplication::initializeGLFW()
{
    // Set error callback
    glfwSetErrorCallback(errorCallback);
    
    // Initialize GLFW
    if (!glfwInit())
    {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }
    
    // Configure GLFW
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    
    return true;
}

bool BakerApplication::createWindow()
{
    // Create window
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, nullptr, nullptr);
    if (!window)
    {
        std::cerr << "Failed to create GLFW window\n";
        return false;
    }
    
    // Make context current
    glfwMakeContextCurrent(window);
    
    // Set callbacks
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    
    // Enable vsync
    glfwSwapInterval(1);
    
    return true;
}

bool BakerApplication::initializeGLAD()
{
    // Load GLAD
    int version = gladLoadGL(glfwGetProcAddress);
    if (!version)
    {
        std::cerr << "Failed to initialize GLAD\n";
        return false;
    }
    
    std::cout << "OpenGL " << GLAD_VERSION_MAJOR(version) << "." << GLAD_VERSION_MINOR(version) << " loaded\n";
    
    // Set initial viewport
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    
    return true;
}

bool BakerApplication::initializeImGui()
{
    // Setup ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Configure ImGui
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    
    // Setup style
    ImGui::StyleColorsDark();
    
    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    
    // Setup platform/renderer backends
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        std::cerr << "Failed to initialize ImGui GLFW backend\n";
        return false;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 460 core"))
    {
        std::cerr << "Failed to initialize ImGui OpenGL backend\n";
        return false;
    }
    
    return true;
}

void BakerApplication::setupOpenGLState()
{
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Set clear color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    
    // Print OpenGL info
    std::cout << "OpenGL Vendor: " << glGetString(GL_VENDOR) << "\n";
    std::cout << "OpenGL Renderer: " << glGetString(GL_RENDERER) << "\n";
    std::cout << "OpenGL Version: " << glGetString(GL_VERSION) << "\n";
    std::cout << "GLSL Version: " << glGetString(GL_SHADING_LANGUAGE_VERSION) << "\n";
}

void BakerApplication::processInput()
{
    // Handle camera input
    if (camera)
    {
        camera->processInput(window, deltaTime);
    }
}

void BakerApplication::update()
{
    // Update camera
    if (camera)
    {
        camera->update(deltaTime);
    }
}

void BakerApplication::render()
{
    // Clear framebuffer
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Get window size
    glm::vec2 windowSize = getWindowSize();
    
    // Start ImGui frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    
    // Render preview to framebuffer
    if (previewRenderer && ui)
    {
        const auto& config = ui->getConfig();
        float currentSizeRatio = ui->getCurrentSizeRatio();
        float currentDistanceRatio = ui->getCurrentDistanceRatio();
        glm::vec2 viewportSize = ui->getViewportSize();
        
        // Update preview renderer mode and mesh
        previewRenderer->setRenderMode(ui->shouldShowMeshPreview());
        previewRenderer->setWireframeMode(ui->shouldShowWireframe());
        
        // Check if mesh changed and force upload if needed
        bool forceUpload = ui->hasMeshChanged();
        previewRenderer->setPreviewMesh(ui->getTestMesh(), forceUpload);
        
        previewRenderer->render(config, currentSizeRatio, currentDistanceRatio, *camera, viewportSize);
        
        // Set the rendered texture for UI display
        ui->setPreviewTexture(previewRenderer->getColorTexture());
    }
    
    // Render UI
    if (ui)
    {
        ui->render(*meshBaker);
    }
    
    // Render ImGui
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    // Handle multi-viewport rendering
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup_current_context = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup_current_context);
    }
}

void BakerApplication::calculateDeltaTime()
{
    float currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;
    
    // Clamp delta time to prevent large jumps
    deltaTime = std::min(deltaTime, 0.1f);
}

bool BakerApplication::shouldClose() const
{
    return window && glfwWindowShouldClose(window);
}

void BakerApplication::framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void BakerApplication::errorCallback(int error, const char* description)
{
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}