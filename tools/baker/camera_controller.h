#pragma once

// Third-party includes
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

/**
 * Camera controller with Space Engineers-style controls
 * Right mouse drag for rotation, WASD for movement, Space/C for up/down, Q/E for roll
 */
class CameraController {
public:
    CameraController();
    ~CameraController() = default;
    
    // Initialization
    bool initialize();
    
    // Update
    void update(float deltaTime);
    void processInput(GLFWwindow* window, float deltaTime);
    
    // Camera properties
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getForward() const { return forward; }
    glm::vec3 getUp() const { return up; }
    glm::vec3 getRight() const { return right; }
    
    // Matrices
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(const glm::vec2& viewportSize) const;
    
    // Camera settings
    void setPosition(const glm::vec3& pos) { position = pos; updateVectors(); }
    void setRotation(float yaw, float pitch, float roll);
    
private:
    // Camera vectors
    glm::vec3 position;
    glm::vec3 forward;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    
    // Euler angles
    float yaw;
    float pitch;
    float roll;
    
    // Camera settings
    float movementSpeed;
    float mouseSensitivity;
    float fov;
    float nearPlane;
    float farPlane;
    
    // Input state
    bool rightMousePressed;
    bool firstMouse;
    glm::vec2 lastMousePos;
    
    // Private methods
    void updateVectors();
    void processMouseMovement(float xOffset, float yOffset);
    void processKeyboard(GLFWwindow* window, float deltaTime);
    void processMouseButton(GLFWwindow* window);
};