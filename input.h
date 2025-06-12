#pragma once
#include <glm/vec2.hpp>
#include <GLFW/glfw3.h>

static bool is_dragging; // This will be used to check if the mouse is being dragged for panning the camera
static glm::vec2 last_mouse_pos;
static GLFWwindow* window;
static bool currentMouseButtons[GLFW_MOUSE_BUTTON_LAST + 1];
static bool previousMouseButtons[GLFW_MOUSE_BUTTON_LAST + 1];
static float scrollDelta;
static bool hasScrolled;

class Input {
private:
public:
    static void init(GLFWwindow* _window);
    static bool isKeyPressed(int key);
    static bool isMouseButtonPressed(int button);
    static glm::vec2 getMousePosition(bool flip_y = true);
    static void update();
    static bool isMouseJustPressed(int button);
    static GLFWwindow* getWindow();
    static float getScrollDelta();
    static bool hasScrollInput();
    static void setScrollCallback();
};
