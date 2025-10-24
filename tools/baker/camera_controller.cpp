#include "camera_controller.h"

// Standard includes
#include <algorithm>
#include <iostream>

CameraController::CameraController()
    : position(0.0f, 0.0f, 5.0f)
    , forward(0.0f, 0.0f, -1.0f)
    , up(0.0f, 1.0f, 0.0f)
    , right(1.0f, 0.0f, 0.0f)
    , worldUp(0.0f, 1.0f, 0.0f)
    , yaw(-90.0f)
    , pitch(0.0f)
    , roll(0.0f)
    , movementSpeed(5.0f)
    , mouseSensitivity(0.1f)
    , fov(45.0f)
    , nearPlane(0.1f)
    , farPlane(1000.0f)
    , rightMousePressed(false)
    , firstMouse(true)
    , lastMousePos(0.0f)
{
}

bool CameraController::initialize()
{
    updateVectors();
    std::cout << "Camera controller initialized\n";
    return true;
}

void CameraController::update(float deltaTime)
{
    // Update camera vectors if needed
    updateVectors();
}

void CameraController::processInput(GLFWwindow* window, float deltaTime)
{
    // Process mouse button state
    processMouseButton(window);
    
    // Process keyboard input
    processKeyboard(window, deltaTime);
    
    // Process mouse movement if right mouse is pressed
    if (rightMousePressed)
    {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        
        if (firstMouse)
        {
            lastMousePos = glm::vec2(mouseX, mouseY);
            firstMouse = false;
        }
        
        float xOffset = static_cast<float>(mouseX) - lastMousePos.x;
        float yOffset = lastMousePos.y - static_cast<float>(mouseY); // Reversed since y-coordinates go from bottom to top
        
        lastMousePos = glm::vec2(mouseX, mouseY);
        
        processMouseMovement(xOffset, yOffset);
    }
}

glm::mat4 CameraController::getViewMatrix() const
{
    return glm::lookAt(position, position + forward, up);
}

glm::mat4 CameraController::getProjectionMatrix(const glm::vec2& viewportSize) const
{
    float aspect = viewportSize.x / viewportSize.y;
    return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
}

void CameraController::setRotation(float newYaw, float newPitch, float newRoll)
{
    yaw = newYaw;
    pitch = newPitch;
    roll = newRoll;
    
    // Constrain pitch
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    
    updateVectors();
}

void CameraController::updateVectors()
{
    // Calculate the new forward vector
    glm::vec3 newForward;
    newForward.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newForward.y = sin(glm::radians(pitch));
    newForward.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    forward = glm::normalize(newForward);
    
    // Calculate right and up vectors
    right = glm::normalize(glm::cross(forward, worldUp));
    
    // Apply roll rotation to the up vector
    glm::vec3 baseUp = glm::normalize(glm::cross(right, forward));
    
    // Create roll rotation matrix
    glm::mat4 rollMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(roll), forward);
    up = glm::vec3(rollMatrix * glm::vec4(baseUp, 0.0f));
    
    // Recalculate right vector with rolled up vector
    right = glm::normalize(glm::cross(forward, up));
}

void CameraController::processMouseMovement(float xOffset, float yOffset)
{
    xOffset *= mouseSensitivity;
    yOffset *= mouseSensitivity;
    
    yaw += xOffset;
    pitch += yOffset;
    
    // Constrain pitch
    pitch = std::clamp(pitch, -89.0f, 89.0f);
    
    updateVectors();
}

void CameraController::processKeyboard(GLFWwindow* window, float deltaTime)
{
    float velocity = movementSpeed * deltaTime;
    
    // WASD movement relative to camera orientation
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= forward * velocity;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= right * velocity;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += right * velocity;
    
    // Space/C for up/down movement relative to camera
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        position += up * velocity;
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS)
        position -= up * velocity;
    
    // Q/E for camera roll
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        roll -= 90.0f * deltaTime; // Roll left
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
        roll += 90.0f * deltaTime; // Roll right
    
    // Update vectors if roll changed
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
    {
        updateVectors();
    }
}

void CameraController::processMouseButton(GLFWwindow* window)
{
    bool currentRightMouseState = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    
    // Check for right mouse button press/release
    if (currentRightMouseState && !rightMousePressed)
    {
        // Right mouse button just pressed
        rightMousePressed = true;
        firstMouse = true;
        
        // Hide cursor
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else if (!currentRightMouseState && rightMousePressed)
    {
        // Right mouse button just released
        rightMousePressed = false;
        
        // Show cursor
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}