#pragma once
#include <imgui.h>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// ImGui constants and helper macros
#ifndef IM_PI
#define IM_PI 3.14159265358979323846f
#endif

#ifndef ImClamp
#define ImClamp(v, mn, mx) ((v) < (mn) ? (mn) : (v) > (mx) ? (mx) : (v))
#endif

#ifndef ImMax
#define ImMax(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef ImRound
#define ImRound(v) ((v) > 0.0f ? (int)((v) + 0.5f) : (int)((v) - 0.5f))
#endif

// State management for circular sliders
struct CircularSliderState {
    std::string text_buffer;
    bool is_active = false;
};

// Core circular slider functions
bool CircularSliderFloat(const char* label, float* v, float v_min, float v_max, 
                        float radius = 50.0f, const char* format = "%.3f", 
                        float align_x = -21.0f, float align_y = 24.0f);

bool CircularSliderInt(const char* label, int* v, int v_min, int v_max,
                      float radius = 50.0f, const char* format = "%d");

// Quaternion trackball controller
bool QuaternionBall(const char* label, glm::quat* orientation, float radius = 80.0f, bool enable_snapping = true);
