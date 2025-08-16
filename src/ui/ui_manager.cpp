#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../core/config.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>

#include "../audio/audio_engine.h"
#include "../scene/scene_manager.h"

// Ensure std::min and std::max are available
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// ============================================================================
// CELL INSPECTOR SECTION
// ============================================================================

void UIManager::renderCellInspector(CellManager &cellManager, SceneManager& sceneManager)
{
    cellManager.setCellLimit(sceneManager.getCurrentCellLimit());
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
	int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Cell Inspector", nullptr, flags))
    {

    if (cellManager.hasSelectedCell())
    {
    	const auto &selectedCell = cellManager.getSelectedCell();
        ImGui::Text("Selected Cell #%d", selectedCell.cellIndex);
        
        ImGui::Separator();

        // Display current properties
        glm::vec3 position = glm::vec3(selectedCell.cellData.positionAndMass);
        glm::vec3 velocity = glm::vec3(selectedCell.cellData.velocity);
        float mass = selectedCell.cellData.positionAndMass.w;
        //float radius = selectedCell.cellData.getRadius();
        int modeIndex = selectedCell.cellData.modeIndex;
        float age = selectedCell.cellData.age;

        ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity.x, velocity.y, velocity.z);
        ImGui::Text("Mass: %.2f", mass);
        //ImGui::Text("Radius: %.2f", radius);
        ImGui::Text("Absolute Mode Index: %i", modeIndex);
        ImGui::Text("Age: %.2f", age);

        ImGui::Separator();
        ImGui::Text("Adhesion Indices:");
        for (int i = 0; i < 20; ++i) {
            int adhesionIndex = selectedCell.cellData.adhesionIndices[i];
            if (adhesionIndex < 0) {
				continue; // Skip invalid indices
            }
            ImGui::SameLine();
            ImGui::Text("%i", adhesionIndex);
        }

        ImGui::Separator();

        // Editable properties
        ImGui::Text("Edit Properties:");

        bool changed = false;
        ComputeCell editedCell = selectedCell.cellData;

        // Position editing
        float pos[3] = {position.x, position.y, position.z};
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            editedCell.positionAndMass.x = pos[0];
            editedCell.positionAndMass.y = pos[1];
            editedCell.positionAndMass.z = pos[2];
            changed = true;
        }

        // Velocity editing
        float vel[3] = {velocity.x, velocity.y, velocity.z};
        if (ImGui::DragFloat3("Velocity", vel, 0.1f))
        {
            editedCell.velocity.x = vel[0];
            editedCell.velocity.y = vel[1];
            editedCell.velocity.z = vel[2];
            changed = true;
        }

        // Mass editing
        if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.1f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            editedCell.positionAndMass.w = mass;
            changed = true;
        }

        // Apply changes
        if (changed)
        {
            cellManager.updateCellData(selectedCell.cellIndex, editedCell);
        }

        ImGui::Separator();

        // Action buttons
        if (ImGui::Button("Clear Selection"))
        {
            cellManager.clearSelection();
        }
        // Status
        if (cellManager.isDraggingCell)
        {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "DRAGGING");
            ImGui::Text("Drag Distance: %.2f", selectedCell.dragDistance);
            ImGui::Text("(Use scroll wheel to adjust distance)");
        }
    }
    else
    {
        ImGui::Text("No cell selected");
        ImGui::Separator();
        ImGui::Text("Instructions:");
        ImGui::BulletText("Left-click on a cell to select it");
        ImGui::BulletText("Drag to move selected cell");
        ImGui::BulletText("Scroll wheel to adjust distance");
        ImGui::BulletText("Selected cell moves in a plane");
        ImGui::BulletText("parallel to the camera");
    }

    }
    ImGui::End();
}

// ============================================================================
// TOOL SELECTOR SECTION
// ============================================================================

void UIManager::drawToolSelector(ToolState &toolState)
{
    const char *tools[] = {"None", "Add", "Edit", "Move (UNIMPLEMENTED)"};
    int current = static_cast<int>(toolState.activeTool);
    if (ImGui::Combo("Tool", &current, tools, IM_ARRAYSIZE(tools)))
    {
        toolState.activeTool = static_cast<ToolType>(current);
    }
}

void UIManager::drawToolSettings(ToolState &toolState, CellManager &cellManager)
{
	switch (toolState.activeTool)
	{
	case ToolType::AddCell:
	    ImGui::ColorEdit4("New Cell Color", &toolState.newCellColor[0], ImGuiColorEditFlags_Float);
	    ImGui::SliderFloat("New Cell Mass", &toolState.newCellMass, 0.1f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
	    break;
	case ToolType::EditCell:

	default:
	    break;
	}
}

// ============================================================================
// UTILITY FUNCTIONS SECTION
// ============================================================================

void UIManager::addTooltip(const char* tooltip)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void UIManager::drawSliderWithInput(const char *label, float *value, float min, float max, const char *format, float step)
{
    ImGui::PushID(label);     // Calculate layout with proper spacing
    float inputWidth = 80.0f; // Increased input field width
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // Calculate slider width: slider + input field
    float sliderWidth = availableWidth - inputWidth - spacing;

    // Label on its own line
    ImGui::Text("%s", label); // Slider with calculated width and step support
    ImGui::PushItemWidth(sliderWidth);
    bool changed = false;
    if (step > 0.0f)
    {
        // For stepped sliders, use float slider but with restricted stepping
        if (ImGui::SliderFloat("##slider", value, min, max, format))
        {
            // Round to nearest step
            *value = min + step * round((*value - min) / step);
            changed = true;
        }
    }
    else
    {
        // Use regular float slider for continuous values
        if (ImGui::SliderFloat("##slider", value, min, max, format))
        {
            changed = true;
        }
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Input field with proper width
    ImGui::PushItemWidth(inputWidth);
    if (step > 0.0f)
    {
        float stepValue = step;
        if (ImGui::InputFloat("##input", value, stepValue, stepValue, format))
        {
            // Round to nearest step
            *value = min + step * round((*value - min) / step);
            changed = true;
        }
    }
    else
    {
        if (ImGui::InputFloat("##input", value, 0.0f, 0.0f, format))
        {
            changed = true;
        }
    }
    ImGui::PopItemWidth();

    // Clamp value to range
    if (*value < min)
        *value = min;
    if (*value > max)
        *value = max;

    // Trigger genome change if any control was modified
    if (changed)
    {
        genomeChanged = true;
    }

    ImGui::PopID();
}

void UIManager::drawSliderWithInput(const char* label, int* value, int min, int max, int step)
{
    ImGui::PushID(label);     // Calculate layout with proper spacing
    float inputWidth = 80.0f; // Increased input field width
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // Calculate slider width: slider + input field
    float sliderWidth = availableWidth - inputWidth - spacing;

    // Label on its own line
    ImGui::Text("%s", label); // Slider with calculated width and step support
    ImGui::PushItemWidth(sliderWidth);
    bool changed = false;
    if (step > 0)
    {
        // For stepped sliders, use int slider but with restricted stepping
        if (ImGui::SliderInt("##slider", value, min, max))
        {
            // Round to nearest step
            *value = min + step * round((*value - min) / step);
            changed = true;
        }
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Input field with proper width
    ImGui::PushItemWidth(inputWidth);
    if (step > 0)
    {
        float stepValue = step;
        if (ImGui::InputInt("##input", value, stepValue, stepValue))
        {
            // Round to nearest step
            *value = min + step * round((*value - min) / step);
            changed = true;
        }
    }
    ImGui::PopItemWidth();

    // Clamp value to range
    if (*value < min)
        *value = min;
    if (*value > max)
        *value = max;

    // Trigger genome change if any control was modified
    if (changed)
    {
        genomeChanged = true;
    }

    ImGui::PopID();
}

void UIManager::drawColorPicker(const char *label, glm::vec3 *color)
{
    // Ensure colors are in the correct 0.0-1.0 range
    glm::vec3 normalizedColor = normalizeColor(*color);
    if (normalizedColor != *color) {
        *color = normalizedColor;
        genomeChanged = true;
    }
    
    float colorArray[3] = {color->r, color->g, color->b};
    if (ImGui::ColorEdit3(label, colorArray, ImGuiColorEditFlags_Float))
    {
        color->r = colorArray[0];
        color->g = colorArray[1];
        color->b = colorArray[2];
        genomeChanged = true;
    }
}

glm::vec3 UIManager::normalizeColor(const glm::vec3& color)
{
    // If any component is > 1.0, assume it's in 0-255 range and convert to 0.0-1.0
    if (color.r > 1.0f || color.g > 1.0f || color.b > 1.0f) {
        return glm::vec3(
            color.r / 255.0f,
            color.g / 255.0f,
            color.b / 255.0f
        );
    }
    return color;
}

void UIManager::validateGenomeColors()
{
    bool colorsFixed = false;
    
    for (auto& mode : currentGenome.modes) {
        glm::vec3 normalizedColor = normalizeColor(mode.color);
        if (normalizedColor != mode.color) {
            mode.color = normalizedColor;
            colorsFixed = true;
        }
    }
    
    if (colorsFixed) {
        genomeChanged = true;
        std::cout << "Fixed color values in genome - converted from 0-255 to 0.0-1.0 range\n";
    }
}

bool UIManager::isColorBright(const glm::vec3 &color)
{
    // Calculate luminance using standard weights for RGB
    // This is the perceived brightness formula
    float luminance = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
    return luminance > 0.5f; // Threshold for considering a color "bright"
}

void UIManager::updatePerformanceMetrics(PerformanceMonitor &perfMonitor, float deltaTime)
{
    // Update frame time statistics
    float frameTimeMs = deltaTime * 1000.0f;

    if (frameTimeMs < perfMonitor.minFrameTime)
        perfMonitor.minFrameTime = frameTimeMs;
    if (frameTimeMs > perfMonitor.maxFrameTime)
        perfMonitor.maxFrameTime = frameTimeMs;

    // Update frame time history
    perfMonitor.frameTimeHistory.push_back(frameTimeMs);
    if (perfMonitor.frameTimeHistory.size() > PerformanceMonitor::HISTORY_SIZE)
        perfMonitor.frameTimeHistory.erase(perfMonitor.frameTimeHistory.begin());

    // Update FPS history
    float currentFPS = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
    perfMonitor.fpsHistory.push_back(currentFPS);
    if (perfMonitor.fpsHistory.size() > PerformanceMonitor::HISTORY_SIZE)
        perfMonitor.fpsHistory.erase(perfMonitor.fpsHistory.begin());

    // Calculate average frame time
    if (!perfMonitor.frameTimeHistory.empty())
    {
        float sum = 0.0f;
        for (float ft : perfMonitor.frameTimeHistory)
            sum += ft;
        perfMonitor.avgFrameTime = sum / perfMonitor.frameTimeHistory.size();
    }

    // Reset min/max periodically (every 5 seconds)
    static float resetTimer = 0.0f;
    resetTimer += deltaTime;
    if (resetTimer >= 5.0f)
    {
        perfMonitor.minFrameTime = 1000.0f;
        perfMonitor.maxFrameTime = 0.0f;
        resetTimer = 0.0f;
    }
}

// ============================================================================
// HELPER FUNCTIONS SECTION
// ============================================================================

// Helper for delta-aware orientation sliders
void UIManager::applyLocalRotation(glm::quat& orientation, const glm::vec3& axis, float delta)
{
    // Apply a local rotation of delta degrees about the given axis
    glm::quat d = glm::angleAxis(glm::radians(delta), axis);
    orientation = glm::normalize(orientation * d);
}