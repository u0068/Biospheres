#include "camera.h"
#include "input.h"
#include <GLFW/glfw3.h>
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 worldUp, float yaw, float pitch)
    : position(position), worldUp(worldUp), yaw(yaw), pitch(pitch), roll(0.0f)
{
    updateCameraVectors();
}

void Camera::processInput(Input& input, float deltaTime)
{
    // Calculate movement speed (with sprint modifier)
    float velocity = moveSpeed * deltaTime;
    if (input.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
        velocity *= sprintMultiplier;    // Movement controls (WASD for forward/back/left/right, Space/C for up/down)
    glm::vec3 moveDirection(0.0f);
    
    if (input.isKeyPressed(GLFW_KEY_W))
        moveDirection += front;
    if (input.isKeyPressed(GLFW_KEY_S))
        moveDirection -= front;
    if (input.isKeyPressed(GLFW_KEY_A))
        moveDirection -= right;
    if (input.isKeyPressed(GLFW_KEY_D))
        moveDirection += right;
    if (input.isKeyPressed(GLFW_KEY_SPACE))
        moveDirection += worldUp; // Move up
    if (input.isKeyPressed(GLFW_KEY_C))
        moveDirection -= worldUp; // Move down

    // Apply movement
    if (glm::length(moveDirection) > 0.0f) {
        position += glm::normalize(moveDirection) * velocity;
    }    // Roll controls (Q and E for camera roll) - reversed directions
    float rollSpeed = 90.0f * deltaTime; // 90 degrees per second
    if (input.isKeyPressed(GLFW_KEY_Q))
        roll += rollSpeed;
    if (input.isKeyPressed(GLFW_KEY_E))
        roll -= rollSpeed;

    // Handle mouse input for camera rotation
    static bool wasRightMousePressed = false;
    bool isRightMousePressed = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    
    if (isRightMousePressed && !wasRightMousePressed) {
        // Start dragging
        isDragging = true;
        lastMousePos = input.getMousePosition(false); // Don't flip Y for mouse tracking
    } else if (!isRightMousePressed && wasRightMousePressed) {
        // Stop dragging
        isDragging = false;
    }
      if (isDragging) {
        glm::vec2 currentMousePos = input.getMousePosition(false);
        glm::vec2 mouseOffset = currentMousePos - lastMousePos;
        lastMousePos = currentMousePos;
        
        processMouseMovement(mouseOffset.x, mouseOffset.y);
    }
    
    wasRightMousePressed = isRightMousePressed;
    
    // Update camera vectors when any rotation has changed
    updateCameraVectors();
}

void Camera::processMouseMovement(float xOffset, float yOffset)
{
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    yaw += xOffset;
    pitch -= yOffset; // Inverted for more intuitive up/down look

    // Constrain pitch to avoid camera flipping
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, position + front, up);
}

void Camera::updateCameraVectors()
{
    // Calculate the new front vector
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    // Re-calculate the right and up vector
    glm::vec3 tempRight = glm::normalize(glm::cross(front, worldUp));
    glm::vec3 tempUp = glm::normalize(glm::cross(tempRight, front));
    
    // Apply roll rotation
    float rollRad = glm::radians(roll);
    right = tempRight * cos(rollRad) + tempUp * sin(rollRad);
    up = tempUp * cos(rollRad) - tempRight * sin(rollRad);
}
