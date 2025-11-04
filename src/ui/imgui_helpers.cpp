#include "imgui_helpers.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

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
                        float radius, const char* format, float align_x, float align_y, bool enable_snapping)
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
            if (enable_snapping) {
                degrees = roundf(degrees / 11.25f) * 11.25f;
            }
            
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
            if (enable_snapping) {
                degrees = roundf(degrees / 11.25f) * 11.25f;
            }
            
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

// Helper function to project mouse position to sphere surface for trackball
static glm::vec3 ProjectToSphere(float x, float y, float radius)
{
    float d = sqrtf(x * x + y * y);
    float t = radius * 0.70710678118654752440f; // radius / sqrt(2)
    float z;

    if (d < t) {
        // Inside sphere
        z = sqrtf(radius * radius - d * d);
    } else {
        // Outside sphere, on hyperbola
        z = t * t / d;
    }

    return glm::normalize(glm::vec3(x, y, z));
}

// Helper function to snap quaternion to nearest grid angles
static glm::quat SnapQuaternionToGrid(const glm::quat& q, float grid_angle_deg)
{
    // Priority order: X-axis (red) first, Y-axis (green) second, Z-axis (blue) computed
    
    glm::mat3 rotation_matrix = glm::mat3_cast(q);
    glm::vec3 x_axis = rotation_matrix * glm::vec3(1, 0, 0);
    glm::vec3 y_axis = rotation_matrix * glm::vec3(0, 1, 0);
    
    float grid_rad = glm::radians(grid_angle_deg);
    int divisions = static_cast<int>(360.0f / grid_angle_deg);
    
    // Step 1: Find the closest grid-aligned direction for X-axis (highest priority)
    glm::vec3 best_x_axis = x_axis;
    float best_x_dot = -1.0f;
    
    for (int lat = -divisions/4; lat <= divisions/4; ++lat) {
        float theta = lat * grid_rad;  // Latitude (pitch)
        for (int lon = 0; lon < divisions; ++lon) {
            float phi = lon * grid_rad;  // Longitude (yaw)
            
            // Convert spherical to cartesian
            glm::vec3 test_dir = glm::vec3(
                cosf(theta) * cosf(phi),
                cosf(theta) * sinf(phi),
                sinf(theta)
            );
            
            float dot = glm::dot(x_axis, test_dir);
            if (dot > best_x_dot) {
                best_x_dot = dot;
                best_x_axis = test_dir;
            }
        }
    }
    best_x_axis = glm::normalize(best_x_axis);
    
    // Step 2: Find the closest grid-aligned direction for Y-axis (second priority)
    // Y-axis must be perpendicular to the snapped X-axis
    glm::vec3 best_y_axis = y_axis;
    float best_y_dot = -1.0f;
    
    for (int lat = -divisions/4; lat <= divisions/4; ++lat) {
        float theta = lat * grid_rad;
        for (int lon = 0; lon < divisions; ++lon) {
            float phi = lon * grid_rad;
            
            glm::vec3 test_dir = glm::vec3(
                cosf(theta) * cosf(phi),
                cosf(theta) * sinf(phi),
                sinf(theta)
            );
            
            // Check if this direction is perpendicular to X-axis
            float perpendicularity = fabsf(glm::dot(best_x_axis, test_dir));
            if (perpendicularity < 0.1f) {  // Nearly perpendicular
                float dot = glm::dot(y_axis, test_dir);
                if (dot > best_y_dot) {
                    best_y_dot = dot;
                    best_y_axis = test_dir;
                }
            }
        }
    }
    
    // If no perpendicular Y was found, project current Y onto plane perpendicular to X
    if (best_y_dot < 0.0f) {
        best_y_axis = y_axis - glm::dot(y_axis, best_x_axis) * best_x_axis;
        if (glm::length(best_y_axis) < 0.001f) {
            // Y is parallel to X, use a default perpendicular vector
            best_y_axis = glm::vec3(0, 0, 1);
            best_y_axis = best_y_axis - glm::dot(best_y_axis, best_x_axis) * best_x_axis;
            if (glm::length(best_y_axis) < 0.001f) {
                best_y_axis = glm::vec3(0, 1, 0);
                best_y_axis = best_y_axis - glm::dot(best_y_axis, best_x_axis) * best_x_axis;
            }
        }
    }
    best_y_axis = glm::normalize(best_y_axis);
    
    // Step 3: Compute Z-axis as cross product (lowest priority, always perpendicular)
    glm::vec3 best_z_axis = glm::cross(best_x_axis, best_y_axis);
    best_z_axis = glm::normalize(best_z_axis);
    
    // Construct rotation matrix from orthonormal basis
    glm::mat3 snapped_matrix;
    snapped_matrix[0] = best_x_axis;
    snapped_matrix[1] = best_y_axis;
    snapped_matrix[2] = best_z_axis;
    
    // Convert back to quaternion
    return glm::normalize(glm::quat_cast(snapped_matrix));
}

bool QuaternionBall(const char* label, glm::quat* orientation, float radius, bool enable_snapping)
{
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    const ImVec2 container_size(radius * 2.5f, radius * 2.5f);
    ImVec2 center = ImVec2(
        cursor_pos.x + container_size.x / 2.0f,
        cursor_pos.y + container_size.y / 2.0f
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Get colors - match orientation gizmo colors
    const ImU32 col_ball = ImGui::GetColorU32(ImGuiCol_SliderGrab);
    const ImU32 col_ball_hovered = ImGui::GetColorU32(ImGuiCol_SliderGrabActive);
    const ImU32 col_axes_x = IM_COL32(80, 120, 255, 255);  // Blue for X (forward)
    const ImU32 col_axes_y = IM_COL32(80, 255, 80, 255);   // Green for Y (right)
    const ImU32 col_axes_z = IM_COL32(255, 80, 80, 255);   // Red for Z (up)
    const ImU32 col_grid = IM_COL32(100, 100, 120, 120);   // Grid lines

    // Check mouse position
    ImVec2 mouse_pos = ImGui::GetMousePos();
    float distance_from_center = sqrtf(
        (mouse_pos.x - center.x) * (mouse_pos.x - center.x) +
        (mouse_pos.y - center.y) * (mouse_pos.y - center.y)
    );
    bool is_mouse_in_ball = distance_from_center <= radius;

    // Draw filled circle with some transparency
    ImU32 ball_fill = ImGui::GetColorU32(ImVec4(0.2f, 0.2f, 0.25f, 0.3f));
    draw_list->AddCircleFilled(center, radius, ball_fill, 64);

    // Draw euler angle grid subdivisions (only if snapping is enabled)
    const int grid_divisions = 16;  // 360/16 = 22.5 degree increments
    const float angle_step = 360.0f / grid_divisions;
    
    if (enable_snapping) {

    // Draw longitude lines (rotation around Y axis)
    for (int i = 0; i < grid_divisions; i++) {
        float angle_deg = i * angle_step;
        float angle_rad = glm::radians(angle_deg);

        // Draw vertical ellipse at this rotation
        for (int j = 0; j < 32; j++) {
            float t1 = (j / 32.0f) * 2.0f * IM_PI;
            float t2 = ((j + 1) / 32.0f) * 2.0f * IM_PI;

            // Calculate 3D points on the sphere
            float x1 = sinf(t1) * cosf(angle_rad);
            float y1 = cosf(t1);
            float z1 = sinf(t1) * sinf(angle_rad);

            float x2 = sinf(t2) * cosf(angle_rad);
            float y2 = cosf(t2);
            float z2 = sinf(t2) * sinf(angle_rad);

            // Project to 2D
            ImVec2 p1 = ImVec2(center.x + x1 * radius, center.y - y1 * radius);
            ImVec2 p2 = ImVec2(center.x + x2 * radius, center.y - y2 * radius);

            // Only draw if in front (positive z)
            if (z1 > 0 && z2 > 0) {
                draw_list->AddLine(p1, p2, col_grid, 1.0f);
            }
        }
    }

    // Draw latitude lines (rotation around X axis)
    for (int i = 1; i < grid_divisions; i++) {
        float angle_deg = i * angle_step;
        float angle_rad = glm::radians(angle_deg - 180.0f);  // Center around equator

        // Draw horizontal circle at this latitude
        float circle_y = sinf(angle_rad);
        float circle_radius = cosf(angle_rad);

        for (int j = 0; j < 32; j++) {
            float t1 = (j / 32.0f) * 2.0f * IM_PI;
            float t2 = ((j + 1) / 32.0f) * 2.0f * IM_PI;

            float x1 = cosf(t1) * circle_radius;
            float z1 = sinf(t1) * circle_radius;
            float x2 = cosf(t2) * circle_radius;
            float z2 = sinf(t2) * circle_radius;

            ImVec2 p1 = ImVec2(center.x + x1 * radius, center.y - circle_y * radius);
            ImVec2 p2 = ImVec2(center.x + x2 * radius, center.y - circle_y * radius);

            // Only draw if in front
            if (z1 > 0 && z2 > 0) {
                draw_list->AddLine(p1, p2, col_grid, 1.0f);
            }
        }
    }
    }  // End of enable_snapping block

    // Draw orientation axes
    glm::mat3 rotation_matrix = glm::mat3_cast(*orientation);

    // Transform basis vectors by current orientation
    glm::vec3 x_axis = rotation_matrix * glm::vec3(1, 0, 0);
    glm::vec3 y_axis = rotation_matrix * glm::vec3(0, 1, 0);
    glm::vec3 z_axis = rotation_matrix * glm::vec3(0, 0, 1);

    // Draw axis with brightness based on depth (brightest at front, fades to back)
    auto draw_axis = [&](const glm::vec3& axis, ImU32 color, float axis_length) {
        const float behind_threshold = -0.01f;  // Small threshold to prevent flickering
        bool is_behind = axis.z < behind_threshold;

        ImVec2 end = ImVec2(
            center.x + axis.x * axis_length,
            center.y - axis.y * axis_length  // Flip Y for screen coordinates
        );

        // Calculate brightness based on depth
        // Brightest when closest (z = +1), gradually fades to back (z = -1)
        // Map z from [-1, 1] to alpha [0.2, 1.0]
        float alpha = (axis.z + 1.0f) / 2.0f;  // Map [-1, 1] to [0, 1]
        alpha = 0.2f + alpha * 0.8f;  // Map [0, 1] to [0.2, 1.0]
        alpha = ImClamp(alpha, 0.2f, 1.0f);
        
        // Line thickness also varies with depth
        float line_thickness = 2.0f + alpha * 2.0f;  // 2.0 to 4.0
        line_thickness = ImClamp(line_thickness, 2.0f, 4.0f);

        ImU32 faded_color = ImGui::GetColorU32(ImVec4(
            ((color >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
            ((color >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
            ((color >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
            alpha
        ));

        if (is_behind) {
            // Draw dotted line for axes behind the plane
            int num_dots = 10;
            for (int i = 0; i < num_dots; i++) {
                if (i % 2 == 0) {
                    float t1 = i / (float)num_dots;
                    float t2 = (i + 1) / (float)num_dots;
                    ImVec2 p1 = ImVec2(
                        center.x + (end.x - center.x) * t1,
                        center.y + (end.y - center.y) * t1
                    );
                    ImVec2 p2 = ImVec2(
                        center.x + (end.x - center.x) * t2,
                        center.y + (end.y - center.y) * t2
                    );
                    draw_list->AddLine(p1, p2, faded_color, line_thickness);
                }
            }
        } else {
            // Solid line for axes in front of the plane
            draw_list->AddLine(center, end, faded_color, line_thickness);
        }

        // Draw endpoint circle - size varies with depth
        float circle_radius = 4.0f + alpha * 2.0f;  // 4.0 to 6.0
        circle_radius = ImClamp(circle_radius, 4.0f, 6.0f);
        draw_list->AddCircleFilled(end, circle_radius, faded_color);
    };

    float axis_length = radius;  // Extend axes to touch the grid

    // Draw axes (positive directions only)
    draw_axis(x_axis, col_axes_x, axis_length);
    draw_axis(y_axis, col_axes_y, axis_length);
    draw_axis(z_axis, col_axes_z, axis_length);

    // Draw outer circle (ball boundary) - on top
    ImU32 ball_color = is_mouse_in_ball ? col_ball_hovered : col_ball;
    draw_list->AddCircle(center, radius, ball_color, 64, 2.0f);

    // Handle mouse interaction - single-axis rotation
    bool changed = false;
    static std::string active_id;
    static int locked_axis = -1;  // -1 = none, 0 = X (pitch), 1 = Y (yaw), 2 = Z (roll)
    static float initial_distance = 0.0f;

    ImVec2 interaction_size = ImVec2(radius * 2.2f, radius * 2.2f);
    ImVec2 interaction_pos = ImVec2(center.x - radius * 1.1f, center.y - radius * 1.1f);
    ImGui::SetCursorScreenPos(interaction_pos);

    std::string button_id = "##qball_" + std::string(label);
    ImGui::InvisibleButton(button_id.c_str(), interaction_size);

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        if (active_id.empty() || active_id == std::string(label)) {
            active_id = std::string(label);

            ImVec2 drag_delta = ImGui::GetIO().MouseDelta;

            if (fabsf(drag_delta.x) > 0.01f || fabsf(drag_delta.y) > 0.01f) {
                // Determine axis lock on first drag
                if (locked_axis == -1) {
                    // Calculate initial distance from center
                    ImVec2 mouse_start = ImVec2(
                        mouse_pos.x - center.x,
                        mouse_pos.y - center.y
                    );
                    initial_distance = sqrtf(mouse_start.x * mouse_start.x + mouse_start.y * mouse_start.y);
                    
                    // Define perimeter zone (outer 30% of radius)
                    float perimeter_threshold = radius * 0.7f;
                    
                    if (initial_distance >= perimeter_threshold) {
                        // Dragging on perimeter = roll (Z-axis rotation)
                        locked_axis = 2;
                    } else {
                        // Dragging in center = pitch or yaw based on direction
                        if (fabsf(drag_delta.x) > fabsf(drag_delta.y)) {
                            locked_axis = 1;  // Lock to Y-axis rotation (yaw)
                        } else {
                            locked_axis = 0;  // Lock to X-axis rotation (pitch)
                        }
                    }
                }

                // Single-axis rotation based on locked axis
                float sensitivity = 0.01f;
                glm::quat rotation;

                if (locked_axis == 2) {
                    // Roll rotation (Z-axis) - calculate tangential movement
                    ImVec2 current_pos = ImVec2(mouse_pos.x - center.x, mouse_pos.y - center.y);
                    ImVec2 prev_pos = ImVec2(
                        current_pos.x - drag_delta.x,
                        current_pos.y - drag_delta.y
                    );
                    
                    // Calculate angles from center
                    float current_angle = atan2f(current_pos.y, current_pos.x);
                    float prev_angle = atan2f(prev_pos.y, prev_pos.x);
                    float angle_delta = current_angle - prev_angle;
                    
                    // Normalize angle delta to [-PI, PI]
                    while (angle_delta > IM_PI) angle_delta -= 2.0f * IM_PI;
                    while (angle_delta < -IM_PI) angle_delta += 2.0f * IM_PI;
                    
                    // Invert rotation direction for more intuitive control
                    rotation = glm::angleAxis(-angle_delta, glm::vec3(0, 0, 1));
                } else if (locked_axis == 1) {
                    // Horizontal drag only = yaw (Y-axis rotation)
                    float angle_y = drag_delta.x * sensitivity;
                    rotation = glm::angleAxis(angle_y, glm::vec3(0, 1, 0));
                } else {
                    // Vertical drag only = pitch (X-axis rotation)
                    float angle_x = drag_delta.y * sensitivity;
                    rotation = glm::angleAxis(angle_x, glm::vec3(1, 0, 0));
                }

                *orientation = glm::normalize(rotation * (*orientation));
                changed = true;
            }
        }
    } else if (ImGui::IsItemDeactivated()) {
        if (active_id == std::string(label)) {
            // Snap to nearest grid intersection on release (only if snapping enabled)
            if (enable_snapping) {
                const float snap_angle = 11.25f;  // Snap at 11.25° (finer than visual grid)
                *orientation = SnapQuaternionToGrid(*orientation, snap_angle);
                changed = true;
            }
            active_id.clear();
            locked_axis = -1;  // Reset axis lock
            initial_distance = 0.0f;  // Reset distance
        }
    }

    // Reset cursor position
    ImGui::SetCursorScreenPos(ImVec2(cursor_pos.x, cursor_pos.y + container_size.y));

    // Calculate latitude and longitude for each axis (reuse existing axis vectors)
    // Convert to spherical coordinates (latitude, longitude)
    auto toSpherical = [](const glm::vec3& v) -> glm::vec2 {
        float latitude = glm::degrees(asinf(v.z));  // Latitude: angle from equator
        float longitude = glm::degrees(atan2f(v.y, v.x));  // Longitude: angle around equator
        return glm::vec2(latitude, longitude);
    };
    
    glm::vec2 x_spherical = toSpherical(x_axis);
    glm::vec2 y_spherical = toSpherical(y_axis);
    glm::vec2 z_spherical = toSpherical(z_axis);

    // Draw color-coded axis key with latitude/longitude (matching gizmo colors)
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 0.47f, 1.0f, 1.0f)); // Blue for X (forward)
    ImGui::Text("X: %.2f°, %.2f°", x_spherical.x, x_spherical.y);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.31f, 1.0f, 0.31f, 1.0f)); // Green for Y (right)
    ImGui::Text("Y: %.2f°, %.2f°", y_spherical.x, y_spherical.y);
    ImGui::PopStyleColor();
    ImGui::SameLine(0, 8);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.31f, 0.31f, 1.0f)); // Red for Z (up)
    ImGui::Text("Z: %.2f°, %.2f°", z_spherical.x, z_spherical.y);
    ImGui::PopStyleColor();

    // Draw label
    if (label && label[0] != '\0' && !(label[0] == '#' && label[1] == '#')) {
        ImGui::SameLine();
        ImGui::TextUnformatted(label);
    }

    return changed;
}