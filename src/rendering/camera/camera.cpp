#include "camera.h"
#include "../../input/input.h"
#include <GLFW/glfw3.h>
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 worldUp, float yaw, float pitch)
    : position(position), worldUp(worldUp), yaw(yaw), pitch(pitch), roll(0.0f)
{
    updateCameraVectors();
}

void Camera::processInput(Input &input, float deltaTime)
{
    // Calculate movement speed (with sprint modifier)
    float velocity = moveSpeed * deltaTime;
    if (input.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
        velocity *= sprintMultiplier;

    // Roll controls (Q and E for camera roll)
    float rollSpeed = 45.0f * deltaTime; // 45 degrees per second
    if (input.isKeyPressed(GLFW_KEY_Q))
    {
        roll += rollSpeed;
        updateCameraVectors();
    }
    if (input.isKeyPressed(GLFW_KEY_E))
    {
        roll -= rollSpeed;
        updateCameraVectors();
    }

    // Handle mouse input for camera rotation
    static bool wasRightMousePressed = false;
    bool isRightMousePressed = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (isRightMousePressed && !wasRightMousePressed)
    {
        // Start dragging
        isDragging = true;
        lastMousePos = input.getMousePosition(false);
        glfwSetInputMode(input.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else if (!isRightMousePressed && wasRightMousePressed)
    {
        // Stop dragging
        isDragging = false;
        glfwSetInputMode(input.getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    if (isDragging)
    {
        glm::vec2 currentMousePos = input.getMousePosition(false);
        glm::vec2 mouseOffset = currentMousePos - lastMousePos;
        lastMousePos = currentMousePos;

        processMouseMovement(mouseOffset.x, mouseOffset.y);
    }

    wasRightMousePressed = isRightMousePressed;

    // Movement controls (WASD for forward/back/left/right, Space/C for up/down)
    // Use camera's actual orientation vectors so movement follows full camera orientation (including roll)
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
        moveDirection += up;
    if (input.isKeyPressed(GLFW_KEY_C))
        moveDirection -= up;

    // Apply movement
    if (glm::length(moveDirection) > 0.0f)
    {
        position += glm::normalize(moveDirection) * velocity;
    }
}

void Camera::processMouseMovement(float xOffset, float yOffset)
{
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;

    // Invert Y axis for natural mouse look
    yOffset = -yOffset;

    // Apply inverted look if enabled
    if (invertLook)
    {
        yOffset = -yOffset;
    }

    // Convert mouse movement to camera's local coordinate system
    // This ensures mouse movement follows the camera's orientation, including roll
    
    // Get the current camera orientation as a rotation matrix
    glm::mat3 cameraRotation = glm::mat3(right, up, front);
    
    // Create rotation matrices for the mouse movement
    // Rotate around the camera's local up axis (horizontal mouse movement)
    glm::mat3 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-xOffset), up);
    // Rotate around the camera's local right axis (vertical mouse movement)  
    glm::mat3 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(yOffset), right);
    
    // Apply rotations to the front vector
    glm::vec3 newFront = pitchRotation * yawRotation * front;
    newFront = glm::normalize(newFront);
    
    // Update Euler angles to match the new front vector
    // Extract yaw from the front vector
    yaw = glm::degrees(atan2(newFront.z, newFront.x));
    
    // Extract pitch from the front vector
    pitch = glm::degrees(asin(newFront.y));
    
    // Clamp pitch to prevent gimbal lock
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    
    // Update camera vectors based on new Euler angles
    updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, position + front, up);
}

void Camera::updateCameraVectors()
{
    // Calculate the base front vector from yaw and pitch (no roll)
    glm::vec3 baseFront;
    baseFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    baseFront.y = sin(glm::radians(pitch));
    baseFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    baseFront = glm::normalize(baseFront);

    // Calculate base right and up vectors (no roll)
    glm::vec3 baseRight = glm::normalize(glm::cross(baseFront, worldUp));
    glm::vec3 baseUp = glm::normalize(glm::cross(baseRight, baseFront));

    // Apply roll rotation to the base vectors
    float rollRad = glm::radians(roll);
    float cosRoll = cos(rollRad);
    float sinRoll = sin(rollRad);

    // Rotate the up and right vectors around the front vector
    right = baseRight * cosRoll + baseUp * sinRoll;
    up = baseUp * cosRoll - baseRight * sinRoll;
    
    // Front vector remains the same (roll doesn't affect where we're looking)
    front = baseFront;
}
