#include "imgui_helpers.h"
#include <cmath>
#include <algorithm>

// Global state management for circular sliders
static std::unordered_map<std::string, CircularSliderState> g_circular_slider_states;

// Helper functions for angle conversion
static float ValueToAngle(float v, float v_min, float v_max)
{
    float normalized = (v - v_min) / (v_max - v_min);
    return normalized * 2.0f * IM_PI;
}

static float AngleToValue(float angle, float v_min, float v_max)
{
    while (angle < 0.0f) angle += 2.0f * IM_PI;
    while (angle >= 2.0f * IM_PI) angle -= 2.0f * IM_PI;
    
    float normalized = angle / (2.0f * IM_PI);
    return v_min + normalized * (v_max - v_min);
}

bool CircularSliderFloat(const char* label, float* v, float v_min, float v_max, 
                        float radius, const char* format, float align_x, float align_y)
{
    // Get or create state for this slider
    std::string widget_id = "circular_slider_" + std::string(label);
    CircularSliderState& state = g_circular_slider_states[widget_id];
    
    // Calculate center position with alignment
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    const ImVec2 container_size(140.0f, 80.0f);
    ImVec2 container_center = ImVec2(
        cursor_pos.x + container_size.x / 2.0f,
        cursor_pos.y + container_size.y / 2.0f
    );
    ImVec2 center = ImVec2(container_center.x + align_x, container_center.y + align_y);
    
    // Get colors
    const ImU32 col_bg = ImGui::GetColorU32(ImGuiCol_FrameBg);
    const ImU32 col_slider = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    const ImU32 col_slider_hovered = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    const ImU32 col_text = ImGui::GetColorU32(ImGuiCol_Text);
    
    // Initialize text buffer if empty
    if (state.text_buffer.empty()) {
        char temp_buf[64];
        sprintf_s(temp_buf, IM_ARRAYSIZE(temp_buf), format, *v);
        state.text_buffer = temp_buf;
    }
    
    // Check mouse position for grab zone
    ImVec2 mouse_pos = ImGui::GetMousePos();
    float distance_from_center = sqrtf(
        (mouse_pos.x - center.x) * (mouse_pos.x - center.x) + 
        (mouse_pos.y - center.y) * (mouse_pos.y - center.y)
    );
    
    // Draw text input first (so it appears on top)
    ImVec2 input_pos = ImVec2(center.x - 30, center.y - 10);
    ImGui::SetCursorScreenPos(input_pos);
    ImGui::PushItemWidth(60);
    std::string input_id = "##input_" + std::string(label);
    
    char text_buf[64];
    strcpy_s(text_buf, IM_ARRAYSIZE(text_buf), state.text_buffer.c_str());
    
    bool text_changed = ImGui::InputText(input_id.c_str(), text_buf, IM_ARRAYSIZE(text_buf), 
        ImGuiInputTextFlags_CharsDecimal | ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    
    // Update state with user input
    if (ImGui::IsItemActive()) {
        state.text_buffer = text_buf;
    }
    
    // Parse text input on Enter or focus loss
    if (text_changed || (ImGui::IsItemDeactivated() && state.is_active)) {
        float new_value = *v;
        if (sscanf_s(text_buf, "%f", &new_value) == 1) {
            *v = ImClamp(new_value, v_min, v_max);
            char temp_buf[64];
            sprintf_s(temp_buf, IM_ARRAYSIZE(temp_buf), format, *v);
            state.text_buffer = temp_buf;
        }
    }
    
    state.is_active = ImGui::IsItemActive();
    bool text_field_is_active = ImGui::IsItemActive();
    
    // Define grab zones
    float inner_radius = 15.0f;
    float outer_radius = radius + 25.0f;
    bool is_mouse_in_grab_zone = (distance_from_center >= inner_radius && 
                                  distance_from_center <= outer_radius) && !text_field_is_active;
    
    // Draw background circle
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 current_slider_color = is_mouse_in_grab_zone && !text_field_is_active ? col_slider_hovered : col_bg;
    draw_list->AddCircle(center, radius, current_slider_color, 0, 3.0f);
    
    // Draw directional arc
    if (fabsf(*v) > 0.001f) {
        float arc_thickness = 8.0f;
        int num_segments = ImMax(32, (int)(radius * 0.5f));
        ImU32 current_arc_color = is_mouse_in_grab_zone && !text_field_is_active ? col_slider_hovered : col_slider;
        
        float start_angle = -IM_PI / 2.0f;
        float end_angle = start_angle + (*v / 180.0f) * IM_PI;
        
        draw_list->PathClear();
        for (int i = 0; i <= num_segments; i++) {
            float angle = start_angle + (end_angle - start_angle) * i / num_segments;
            ImVec2 point = ImVec2(
                center.x + cosf(angle) * radius,
                center.y + sinf(angle) * radius
            );
            draw_list->PathLineTo(point);
        }
        draw_list->PathStroke(current_arc_color, 0, arc_thickness);
    }
    
    // Draw handle
    float handle_radius = 6.0f;
    float handle_angle = -IM_PI / 2.0f + (*v / 180.0f) * IM_PI;
    ImVec2 handle_pos = ImVec2(
        center.x + cosf(handle_angle) * radius,
        center.y + sinf(handle_angle) * radius
    );
    ImU32 handle_color = is_mouse_in_grab_zone && !text_field_is_active ? col_slider_hovered : col_slider;
    draw_list->AddCircleFilled(handle_pos, handle_radius, handle_color);
    
    // Handle mouse interaction
    bool changed = false;
    if (!text_field_is_active) {
        // Create invisible button for interaction
        ImVec2 button_size = ImVec2(outer_radius * 2.0f, outer_radius * 2.0f);
        ImVec2 button_pos = ImVec2(center.x - outer_radius, center.y - outer_radius);
        ImGui::SetCursorScreenPos(button_pos);
        
        std::string button_id = "##button_" + std::string(label);
        bool button_clicked = ImGui::InvisibleButton(button_id.c_str(), button_size, ImGuiButtonFlags_MouseButtonLeft);
        bool button_active = ImGui::IsItemActive();
        
        // Restore cursor position
        ImGui::SetCursorScreenPos(cursor_pos);
        
        // Handle dragging
        if (button_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            mouse_pos.x -= center.x;
            mouse_pos.y -= center.y;
            float mouse_angle = atan2f(mouse_pos.y, mouse_pos.x) + IM_PI / 2.0f;
            
            float degrees = mouse_angle * 180.0f / IM_PI;
            if (degrees > 180.0f) degrees -= 360.0f;
            degrees = roundf(degrees / 15.0f) * 15.0f;
            
            if (fabsf(degrees - *v) > 0.001f) {
                *v = ImClamp(degrees, v_min, v_max);
                changed = true;
                char temp_buf[64];
                sprintf_s(temp_buf, IM_ARRAYSIZE(temp_buf), format, *v);
                state.text_buffer = temp_buf;
            }
        }
        
        // Handle initial click
        else if (button_clicked && is_mouse_in_grab_zone) {
            ImVec2 mouse_pos = ImGui::GetMousePos();
            mouse_pos.x -= center.x;
            mouse_pos.y -= center.y;
            float mouse_angle = atan2f(mouse_pos.y, mouse_pos.x) + IM_PI / 2.0f;
            
            float degrees = mouse_angle * 180.0f / IM_PI;
            if (degrees > 180.0f) degrees -= 360.0f;
            degrees = roundf(degrees / 15.0f) * 15.0f;
            
            if (fabsf(degrees - *v) > 0.001f) {
                *v = ImClamp(degrees, v_min, v_max);
                changed = true;
                char temp_buf[64];
                sprintf_s(temp_buf, IM_ARRAYSIZE(temp_buf), format, *v);
                state.text_buffer = temp_buf;
            }
        }
    }
    
    // Draw label if not hidden
    if (label && label[0] != '\0' && !(label[0] == '#' && label[1] == '#')) {
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    }
    
    return changed;
}

bool CircularSliderInt(const char* label, int* v, int v_min, int v_max, 
                      float radius, const char* format)
{
    float float_val = (float)*v;
    bool changed = CircularSliderFloat(label, &float_val, (float)v_min, (float)v_max, radius, format);
    if (changed) {
        *v = (int)ImRound(float_val);
    }
    return changed;
}