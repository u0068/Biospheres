#include "camera.h"
#include "../../input/input.h"
#include <GLFW/glfw3.h>
#include <algorithm>

Camera::Camera(glm::vec3 position, glm::vec3 worldUp, float yaw, float pitch)
    : position(position), worldUp(worldUp), roll(0.0f)
{
    // Initialize camera orientation from yaw and pitch
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);
    
    // Initialize right and up vectors
    right = glm::normalize(glm::cross(front, worldUp));
    up = glm::normalize(glm::cross(right, front));
}

void Camera::processInput(Input &input, float deltaTime)
{
    // Calculate movement speed (with sprint modifier)
    float velocity = moveSpeed * deltaTime;
    if (input.isKeyPressed(GLFW_KEY_LEFT_SHIFT))
        velocity *= sprintMultiplier;

    // Handle mouse input for camera rotation (Space Engineers style)
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

    // Roll controls (Q and E for camera roll around forward vector)
    float rollSpeed = 90.0f * deltaTime; // 90 degrees per second
    if (input.isKeyPressed(GLFW_KEY_Q))
    {
        roll += rollSpeed;
        // Apply roll rotation directly to right and up vectors
        float rollRad = glm::radians(rollSpeed);
        float cosRoll = cos(rollRad);
        float sinRoll = sin(rollRad);
        
        glm::vec3 oldRight = right;
        right = right * cosRoll + up * sinRoll;
        up = up * cosRoll - oldRight * sinRoll;
    }
    if (input.isKeyPressed(GLFW_KEY_E))
    {
        roll -= rollSpeed;
        // Apply roll rotation directly to right and up vectors
        float rollRad = glm::radians(-rollSpeed);
        float cosRoll = cos(rollRad);
        float sinRoll = sin(rollRad);
        
        glm::vec3 oldRight = right;
        right = right * cosRoll + up * sinRoll;
        up = up * cosRoll - oldRight * sinRoll;
    }

    // Handle mouse scroll for zoom (Space Engineers style)
    if (input.hasScrollInput())
    {
        float scrollDelta = input.getScrollDelta();
        processMouseScroll(scrollDelta);
    }

    // Movement controls (Space Engineers style - all movement relative to camera orientation)
    // WASD for forward/back/left/right, Space/C for up/down relative to camera view
    glm::vec3 moveDirection(0.0f);

    // Forward/backward movement (W/S) - relative to camera's forward direction
    if (input.isKeyPressed(GLFW_KEY_W))
        moveDirection += front;
    if (input.isKeyPressed(GLFW_KEY_S))
        moveDirection -= front;

    // Left/right movement (A/D) - relative to camera's right direction
    if (input.isKeyPressed(GLFW_KEY_A))
        moveDirection -= right;
    if (input.isKeyPressed(GLFW_KEY_D))
        moveDirection += right;

    // Up/down movement (Space/C) - relative to camera's up direction
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

    // Invert Y axis for natural mouse look (Space Engineers style)
    yOffset = -yOffset;

    // Apply inverted look if enabled
    if (invertLook)
    {
        yOffset = -yOffset;
    }

    // Apply camera-relative rotations directly to orientation vectors
    // This eliminates any world-relative bias and allows true 6DOF movement
    
    // Rotate around the camera's current up axis (horizontal mouse movement)
    glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-xOffset), up);
    
    // Rotate around the camera's current right axis (vertical mouse movement)  
    glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(yOffset), right);
    
    // Apply rotations to all orientation vectors
    front = glm::vec3(pitchRotation * yawRotation * glm::vec4(front, 0.0f));
    front = glm::normalize(front);
    
    // Update right and up vectors to maintain orthogonality
    right = glm::vec3(pitchRotation * yawRotation * glm::vec4(right, 0.0f));
    right = glm::normalize(right);
    
    up = glm::vec3(pitchRotation * yawRotation * glm::vec4(up, 0.0f));
    up = glm::normalize(up);
}

void Camera::processMouseScroll(float yOffset)
{
    // Space Engineers style zoom - move camera forward/backward along view direction
    float zoomDistance = zoomSpeed * yOffset * 0.01f; // Scale down for smoother zoom
    
    // Calculate new position
    glm::vec3 newPosition = position + front * zoomDistance;
    
    // Clamp distance to prevent getting too close or too far
    float distanceFromOrigin = glm::length(newPosition);
    if (distanceFromOrigin >= minDistance && distanceFromOrigin <= maxDistance)
    {
        position = newPosition;
    }
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, position + front, up);
}


