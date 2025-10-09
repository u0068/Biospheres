#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 10.0f), 
           glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f), 
           float yaw = -90.0f, 
           float pitch = 0.0f);

    // Main input processing
    void processInput(class Input& input, float deltaTime, bool allowScroll = true);
    void processMouseMovement(float xOffset, float yOffset);
    void processMouseScroll(float yOffset);

    // Getters
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    glm::vec3 getRight() const { return right; }
    glm::vec3 getUp() const { return up; }
    glm::mat4 getViewMatrix() const;

    // Camera settings (Space Engineers style)
    float moveSpeed = 15.0f;           // Base movement speed
    float sprintMultiplier = 3.0f;     // Sprint speed multiplier
    float mouseSensitivity = 0.1f;     // Mouse look sensitivity
    float zoomSpeed = 50.0f;           // Zoom speed
    float minDistance = 0.5f;          // Minimum zoom distance
    float maxDistance = 1000.0f;       // Maximum zoom distance
    bool invertLook = false;           // Invert mouse Y axis

private:
    // Camera attributes
    glm::vec3 position;
    glm::vec3 worldUp;

    // Camera orientation (space-like, no world-relative constraints)
    glm::vec3 front;     // Direction camera is facing
    glm::vec3 right;     // Camera's right vector
    glm::vec3 up;        // Camera's up vector
    float roll;          // Roll around forward vector

    // Mouse tracking
    bool isDragging = false;
    glm::vec2 lastMousePos;
};
