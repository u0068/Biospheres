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

void UIManager::renderCameraControls(CellManager &cellManager, Camera &camera, SceneManager& sceneManager)
{
    cellManager.setCellLimit(sceneManager.getCurrentCellLimit());
    ImGui::SetNextWindowPos(ImVec2(50, 470), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);
	int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Camera & Controls", nullptr, flags))
    {
    glm::vec3 camPos = camera.getPosition();
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
    ImGui::Separator();
    
    // Window lock/unlock button
    ImGui::Text("Window Management:");
    if (ImGui::Button(windowsLocked ? "Unlock All Windows" : "Lock All Windows"))
    {
        windowsLocked = !windowsLocked;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Lock/unlock position and size of all UI windows");
    }
    ImGui::Separator();
    
    ImGui::Text("Camera Controls:");
    ImGui::BulletText("WASD - Move");
    ImGui::BulletText("Q/E - Roll");
    ImGui::BulletText("Space/C - Up/Down");
    ImGui::BulletText("Right-click + Drag - Look");
    ImGui::Separator();
    ImGui::Text("Cell Interaction:");
    ImGui::BulletText("Left-click - Select cell");
    ImGui::BulletText("Left-click + Drag - Move selected cell");
    ImGui::BulletText("Scroll Wheel - Adjust drag distance");
    
    ImGui::Separator();
    ImGui::Text("Visualization:");
    ImGui::Checkbox("Show Orientation Gizmos", &showOrientationGizmos);
    addTooltip("Display forward (red), up (green), and right (blue) orientation axes for each cell");
    
    ImGui::Checkbox("Show Adhesion Lines", &showAdhesionLines);
    addTooltip("Display orange lines connecting sibling cells when their parent has adhesionSettings enabled");
    
    ImGui::Checkbox("Wireframe Mode", &wireframeMode);
    addTooltip("Render cells in wireframe mode to verify back face culling is working");
    
    ImGui::Checkbox("Frustum Culling", &enableFrustumCulling);
    addTooltip("Enable frustum culling to improve performance by only rendering visible cells");
    
    ImGui::Checkbox("Distance Culling & Fading", &enableDistanceCulling);
    addTooltip("Enable distance-based culling and fading for cells far from camera");
    
    // Sync settings with cell manager
            cellManager.useFrustumCulling = enableFrustumCulling;
        cellManager.useDistanceCulling = enableDistanceCulling;
        cellManager.invalidateStatisticsCache(); // Invalidate cache since culling settings changed
    
    // Distance culling parameters (only show if distance culling is enabled)
    if (enableDistanceCulling) {
        ImGui::Separator();
        ImGui::Text("Distance Culling Parameters:");
        
        // Get current values from cell manager
        float maxDistance = cellManager.getMaxRenderDistance();
        float fadeStart = cellManager.getFadeStartDistance();
        float fadeEnd = cellManager.getFadeEndDistance();
        
        if (ImGui::DragFloat("Max Render Distance", &maxDistance, 10.0f, 100.0f, 1000.0f, "%.0f")) {
            cellManager.setDistanceCullingParams(maxDistance, fadeStart, fadeEnd);
        }
        addTooltip("Maximum distance from camera to render cells");
        
        if (ImGui::DragFloat("Fade Start Distance", &fadeStart, 10.0f, 50.0f, maxDistance - 50.0f, "%.0f")) {
            cellManager.setDistanceCullingParams(maxDistance, fadeStart, fadeEnd);
        }
        addTooltip("Distance where cells start to fade out");
        
        if (ImGui::DragFloat("Fade End Distance", &fadeEnd, 10.0f, fadeStart + 50.0f, maxDistance, "%.0f")) {
            cellManager.setDistanceCullingParams(maxDistance, fadeStart, fadeEnd);
        }
        addTooltip("Distance where cells become completely invisible");
        
        // Fog color control
        ImGui::Separator();
        ImGui::Text("Fog Color:");
        glm::vec3 fogColor = cellManager.getFogColor();
        if (ImGui::ColorEdit3("##FogColor", &fogColor.x, ImGuiColorEditFlags_Float)) {
            cellManager.setFogColor(fogColor);
        }
        addTooltip("Color of atmospheric fog for distant cells");
    }

    // Show current selection info
    if (cellManager.hasSelectedCell())    {
        ImGui::Separator();
        const auto &selection = cellManager.getSelectedCell();
        ImGui::Text("Selected: Cell #%d", selection.cellIndex);
        ImGui::Text("Drag Distance: %.1f", selection.dragDistance);
    }
    }
    ImGui::End();
}

// Helper method to get window flags based on lock state
int UIManager::getWindowFlags(int baseFlags) const
{
    if (windowsLocked)
    {
        // Remove AlwaysAutoResize flag if present since it conflicts with NoResize
        int lockedFlags = baseFlags & ~ImGuiWindowFlags_AlwaysAutoResize;
        return lockedFlags | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }
    return baseFlags;
}
