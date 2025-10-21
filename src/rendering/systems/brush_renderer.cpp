#include "brush_renderer.h"
#include "../core/shader_class.h"
#include "../camera/camera.h"
#include "../../input/injection_system.h"
#include <iostream>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

BrushRenderer::BrushRenderer() {
}

BrushRenderer::~BrushRenderer() {
    cleanup();
}

void BrushRenderer::initialize() {
    if (initialized) return;
    
    try {
        std::cout << "BrushRenderer: Starting initialization..." << std::endl;
        
        // Create brush shader
        std::cout << "BrushRenderer: Loading shaders..." << std::endl;
        brushShader = new Shader("shaders/rendering/debug/brush.vert", "shaders/rendering/debug/brush.frag");
        std::cout << "BrushRenderer: Shaders loaded successfully" << std::endl;
        
        // Generate sphere geometry
        std::cout << "BrushRenderer: Generating sphere geometry..." << std::endl;
        generateSphereGeometry(16); // 16 segments for reasonable quality
        std::cout << "BrushRenderer: Generated " << vertices.size() << " vertices and " << indices.size() << " indices" << std::endl;
        
        // Setup OpenGL buffers
        std::cout << "BrushRenderer: Setting up OpenGL buffers..." << std::endl;
        setupBuffers();
        std::cout << "BrushRenderer: OpenGL buffers set up successfully" << std::endl;
        
        initialized = true;
        std::cout << "BrushRenderer: Initialized successfully" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "BrushRenderer: Failed to initialize - " << e.what() << std::endl;
        cleanup();
    }
    catch (...) {
        std::cerr << "BrushRenderer: Failed to initialize - Unknown exception" << std::endl;
        cleanup();
    }
}

void BrushRenderer::cleanup() {
    if (brushShader) {
        delete brushShader;
        brushShader = nullptr;
    }
    
    if (VAO) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    if (VBO) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (EBO) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    
    initialized = false;
}

void BrushRenderer::renderBrush(const glm::vec3& position, float radius, InjectionMode mode, 
                               const Camera& camera, const glm::vec2& screenSize, bool isInjecting) {
    if (!initialized || !brushShader) return;
    
    // Enable blending for translucency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Disable depth writing but keep depth testing for proper sorting
    glDepthMask(GL_FALSE);
    
    brushShader->use();
    
    // Set up matrices
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::scale(model, glm::vec3(radius));
    
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), screenSize.x / screenSize.y, 0.1f, 1000.0f);
    
    // Set uniforms
    brushShader->setMat4("u_modelMatrix", model);
    brushShader->setMat4("u_viewMatrix", view);
    brushShader->setMat4("u_projectionMatrix", projection);
    
    // Set brush color based on injection mode and state
    glm::vec4 brushColor = getBrushColor(mode, isInjecting);
    brushShader->setVec4("u_brushColor", brushColor.x, brushColor.y, brushColor.z, brushColor.w);
    
    // Render the sphere
    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

glm::vec4 BrushRenderer::getBrushColor(InjectionMode mode, bool isInjecting) const {
    // Requirement 6.2: Use different brush colors for density vs velocity injection modes
    // Highlight brush when actively injecting
    
    glm::vec4 baseColor;
    switch (mode) {
        case InjectionMode::Density:
            baseColor = glm::vec4(1.0f, 0.5f, 0.0f, 0.3f); // Orange with transparency
            break;
        case InjectionMode::Velocity:
            baseColor = glm::vec4(0.0f, 0.5f, 1.0f, 0.3f); // Blue with transparency
            break;
        case InjectionMode::CellSelection:
        default:
            baseColor = glm::vec4(0.5f, 0.5f, 0.5f, 0.1f); // Gray (should not be visible)
            break;
    }
    
    // Highlight when injecting - increase brightness and opacity
    if (isInjecting) {
        baseColor.r = std::min(1.0f, baseColor.r * 1.5f);
        baseColor.g = std::min(1.0f, baseColor.g * 1.5f);
        baseColor.b = std::min(1.0f, baseColor.b * 1.5f);
        baseColor.a = std::min(1.0f, baseColor.a * 2.0f);
    }
    
    return baseColor;
}

void BrushRenderer::generateSphereGeometry(int segments) {
    vertices.clear();
    indices.clear();
    
    // Generate sphere vertices using spherical coordinates
    for (int i = 0; i <= segments; ++i) {
        float phi = static_cast<float>(i) / segments * M_PI; // 0 to PI
        
        for (int j = 0; j <= segments; ++j) {
            float theta = static_cast<float>(j) / segments * 2.0f * M_PI; // 0 to 2*PI
            
            // Calculate position
            float x = static_cast<float>(sin(phi) * cos(theta));
            float y = static_cast<float>(cos(phi));
            float z = static_cast<float>(sin(phi) * sin(theta));
            
            // Add vertex (position only for now)
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);
        }
    }
    
    // Generate indices for triangles
    for (int i = 0; i < segments; ++i) {
        for (int j = 0; j < segments; ++j) {
            int first = i * (segments + 1) + j;
            int second = first + segments + 1;
            
            // First triangle
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            // Second triangle
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
}

void BrushRenderer::setupBuffers() {
    // Generate and bind VAO
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // Generate and setup VBO
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    
    // Generate and setup EBO
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    // Setup vertex attributes (position only)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Unbind
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}