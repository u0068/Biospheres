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

    // Handle mouse input for camera rotation FIRST
    static bool wasRightMousePressed = false;
    bool isRightMousePressed = input.isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
    if (isRightMousePressed && !wasRightMousePressed)
    {
        // Start dragging
        isDragging = true;
        lastMousePos = input.getMousePosition(false); // Don't flip Y for mouse tracking
        // Hide cursor when starting to drag
        glfwSetInputMode(input.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }
    else if (!isRightMousePressed && wasRightMousePressed)
    {
        // Stop dragging
        isDragging = false; // Show cursor when stopping drag
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

    // Roll controls (Q and E for camera roll) - handle AFTER mouse movement
    float rollSpeed = 45.0f * deltaTime; // 45 degrees per second (reduced from 90)
    bool rollChanged = false;
    if (input.isKeyPressed(GLFW_KEY_Q))
    {
        roll += rollSpeed;
        rollChanged = true;
    }
    if (input.isKeyPressed(GLFW_KEY_E))
    {
        roll -= rollSpeed;
        rollChanged = true;
    }

    // Update camera vectors only if roll changed (mouse movement handles its own vector updates)
    if (rollChanged)
    {
        updateCameraVectors();
    }

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
        moveDirection += up; // Move up relative to camera orientation
    if (input.isKeyPressed(GLFW_KEY_C))
        moveDirection -= up; // Move down relative to camera orientation

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

    // Invert Y axis for natural mouse look (moving mouse up should look up)
    yOffset = -yOffset;

    // Apply inverted look if enabled (double inversion)
    if (invertLook)
    {
        yOffset = -yOffset;
    }

    // Use camera-relative axes for rotation
    // Rotate around the camera's local up axis (horizontal mouse movement)
    glm::mat4 yawRotation = glm::rotate(glm::mat4(1.0f), glm::radians(-xOffset), up);

    // Rotate around the camera's local right axis (vertical mouse movement)
    glm::mat4 pitchRotation = glm::rotate(glm::mat4(1.0f), glm::radians(yOffset), right);

    // Apply pitch rotation first, then yaw rotation to the front vector
    glm::vec4 newFront = yawRotation * pitchRotation * glm::vec4(front, 0.0f);
    front = glm::normalize(glm::vec3(newFront));

    // Also rotate the up vector to maintain proper orientation
    glm::vec4 newUp = yawRotation * pitchRotation * glm::vec4(up, 0.0f);
    up = glm::normalize(glm::vec3(newUp));

    // Update right vector based on new front and up
    right = glm::normalize(glm::cross(front, up));

    // Update Euler angles to match the new orientation (for consistency)
    // Extract yaw from the front vector
    yaw = glm::degrees(atan2(front.z, front.x));

    // Extract pitch from the front vector
    pitch = glm::degrees(asin(front.y));
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(position, position + front, up);
}

void Camera::updateCameraVectors()
{
    // Calculate the new front vector from Euler angles
    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);

    // Re-calculate the right and up vector using world-relative orientation
    glm::vec3 tempRight = glm::normalize(glm::cross(front, worldUp));
    glm::vec3 tempUp = glm::normalize(glm::cross(tempRight, front));

    // Apply roll rotation to the temporary vectors
    float rollRad = glm::radians(roll);
    right = tempRight * cos(rollRad) + tempUp * sin(rollRad);
    up = tempUp * cos(rollRad) - tempRight * sin(rollRad);
}
