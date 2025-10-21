#pragma once

#include <glm/glm.hpp>
#include <glad/glad.h>
#include <vector>

// Forward declarations
class Camera;
class Shader;
enum class InjectionMode;

class BrushRenderer {
private:
    // OpenGL resources
    GLuint VAO{0};
    GLuint VBO{0};
    GLuint EBO{0};
    
    // Shader for brush rendering
    Shader* brushShader = nullptr;
    
    // Sphere geometry data
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
    
    // Brush properties
    bool initialized{false};
    
public:
    BrushRenderer();
    ~BrushRenderer();
    
    // Lifecycle
    void initialize();
    void cleanup();
    
    // Rendering
    void renderBrush(const glm::vec3& position, float radius, InjectionMode mode, 
                    const Camera& camera, const glm::vec2& screenSize, bool isInjecting = false);
    
    // Utility
    glm::vec4 getBrushColor(InjectionMode mode, bool isInjecting = false) const;
    
private:
    void generateSphereGeometry(int segments = 16);
    void setupBuffers();
};