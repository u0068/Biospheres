#pragma once
#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class Camera {
public:
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 10.0f), 
           glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f), 
           float yaw = -90.0f, 
           float pitch = 0.0f);

    // Main input processing
    void processInput(class Input& input, float deltaTime);
    void processMouseMovement(float xOffset, float yOffset);
    void processMouseScroll(float yOffset);

    // Getters
    glm::vec3 getPosition() const { return position; }
    glm::vec3 getFront() const { return front; }
    glm::vec3 getRight() const { return right; }
    glm::vec3 getUp() const { return up; }
    glm::mat4 getViewMatrix() const;    // Camera settings
    float moveSpeed = 10.0f;
    float sprintMultiplier = 2.0f;
    float mouseSensitivity = 0.5f;
    float zoomSpeed = 200.0f;
    float minDistance = 1.0f;
    float maxDistance = 100.0f;
    bool invertLook = false;

private:
    // Camera attributes
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;    // Euler angles
    float yaw;   // Y-axis rotation
    float pitch; // X-axis rotation
    float roll;  // Z-axis rotation

    // Mouse tracking
    bool isDragging = false;
    glm::vec2 lastMousePos;

    // Update camera vectors based on updated Euler angles
    void updateCameraVectors();
};
