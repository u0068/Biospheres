#include "input.h"
#include <iostream>

void Input::init(GLFWwindow* _window)
{
    window = _window;
	is_dragging = false;
    last_mouse_pos = { 0.0f, 0.0f };
    // Set the input mode to capture the mouse cursor
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

bool Input::isKeyPressed(int key)
{
    return glfwGetKey(window, key) == GLFW_PRESS;
}

bool Input::isMouseButtonPressed(int button)
{
    return glfwGetMouseButton(window, button) == GLFW_PRESS;
}

bool Input::isMouseJustPressed(int button) {
	return currentMouseButtons[button] && !previousMouseButtons[button]; // Check if the button is currently pressed and was not pressed in the previous frame
}

glm::vec2 Input::getMousePosition(bool flip_y) // Flips Y axis for OpenGL coordinates
{
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    if (flip_y) {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        return { static_cast<float>(x), static_cast<float>(height) - static_cast<float>(y) };
    }
    else
    {
		return { static_cast<float>(x), static_cast<float>(y) };
    }
}

void Input::update() {
    for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button) {
        previousMouseButtons[button] = currentMouseButtons[button];
        currentMouseButtons[button] = glfwGetMouseButton(window, button) == GLFW_PRESS;
    }
}
