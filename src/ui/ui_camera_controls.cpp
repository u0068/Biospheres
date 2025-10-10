#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../rendering/camera/camera.h"
#include "../core/config.h"
#include "imgui.h"
#include "ui_layout.h"
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
    ImGui::SetNextWindowPos(UILayout::Layout::getCameraControlsPos(), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(UILayout::Layout::getCameraControlsSize(), ImGuiCond_FirstUseEver);
	int flags = getWindowFlags();
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
    
    ImGui::Separator();
    ImGui::Text("Enhanced Diagnostics:");
    
    // Main diagnostic toggle
    if (ImGui::Button(cellManager.diagnosticsRunning ? "Stop Enhanced Diagnostics" : "Start Enhanced Diagnostics"))
    {
        cellManager.toggleEnhancedDiagnostics();
    }
    addTooltip("Start/stop comprehensive diagnostic recording including adhesion events, cell lifecycle, physics events, and genome tracking.");
    
    // Diagnostic settings (only show when diagnostics are running)
    if (cellManager.diagnosticsRunning) {
        ImGui::Indent();
        
        // Event type toggles
        ImGui::Text("Event Types:");
        ImGui::Checkbox("Adhesion Events", &cellManager.diagnosticState.adhesionEventsEnabled);
        addTooltip("Record adhesion connection/disconnection events");
        
        ImGui::Checkbox("Cell Lifecycle Events", &cellManager.diagnosticState.cellLifecycleEventsEnabled);
        addTooltip("Record cell birth, death, splitting, and mode changes");
        
        ImGui::Checkbox("Physics Events", &cellManager.diagnosticState.physicsEventsEnabled);
        addTooltip("Record high velocity, acceleration, and physics instability events");
        
        ImGui::Checkbox("System Events", &cellManager.diagnosticState.systemEventsEnabled);
        addTooltip("Record buffer overflows and performance warnings");
        
        ImGui::Checkbox("Genome Tracking", &cellManager.diagnosticState.genomeTrackingEnabled);
        addTooltip("Track genome differences from default values for each cell");
        
        ImGui::Checkbox("Lineage Tracking", &cellManager.diagnosticState.lineageTrackingEnabled);
        addTooltip("Track cell lineage relationships and family trees");
        
        ImGui::Checkbox("Real-time Monitoring", &cellManager.diagnosticState.realTimeMonitoringEnabled);
        addTooltip("Enable real-time performance threshold monitoring");
        
        // Performance thresholds (only show if physics events or real-time monitoring enabled)
        if (cellManager.diagnosticState.physicsEventsEnabled || cellManager.diagnosticState.realTimeMonitoringEnabled) {
            ImGui::Separator();
            ImGui::Text("Performance Thresholds:");
            
            ImGui::DragFloat("Velocity Threshold", &cellManager.diagnosticState.velocityThreshold, 1.0f, 10.0f, 200.0f, "%.1f");
            addTooltip("Velocity threshold for high velocity events");
            
            ImGui::DragFloat("Acceleration Threshold", &cellManager.diagnosticState.accelerationThreshold, 5.0f, 50.0f, 500.0f, "%.1f");
            addTooltip("Acceleration threshold for high acceleration events");
            
            ImGui::DragFloat("Toxin Threshold", &cellManager.diagnosticState.toxinThreshold, 0.01f, 0.1f, 1.0f, "%.2f");
            addTooltip("Toxin level threshold for instability events");
        }
        
        // Buffer status
        ImGui::Separator();
        ImGui::Text("Buffer Status:");
        float bufferUsage = 100.0f * cellManager.diagnosticState.currentEntries / cellManager.diagnosticState.maxEntries;
        ImGui::ProgressBar(bufferUsage / 100.0f, ImVec2(-1, 0), (std::to_string(static_cast<int>(bufferUsage)) + "%").c_str());
        
        if (cellManager.diagnosticState.bufferOverflowOccurred) {
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WARNING: Buffer overflow occurred!");
        }
        
        ImGui::Text("Entries: %u / %u", cellManager.diagnosticState.currentEntries, cellManager.diagnosticState.maxEntries);
        
        // Real-time event display
        if (cellManager.diagnosticState.realTimeMonitoringEnabled) {
            ImGui::Separator();
            ImGui::Text("Recent Events:");
            auto recentEvents = cellManager.getRecentEvents(10);
            
            if (recentEvents.empty()) {
                ImGui::TextDisabled("No recent events");
            } else {
                ImGui::BeginChild("RecentEvents", ImVec2(0, 100), true);
                for (const auto& event : recentEvents) {
                    ImGui::TextWrapped("%s", event.c_str());
                }
                ImGui::EndChild();
            }
        }
        
        // Lineage statistics display
        if (cellManager.diagnosticState.lineageTrackingEnabled) {
            ImGui::Separator();
            ImGui::Text("Lineage Statistics:");
            
            // Sync lineage tracking data from GPU before displaying
            cellManager.syncLineageTrackingFromGPU();
            std::string lineageStats = cellManager.getLineageStatistics();
            if (lineageStats.empty()) {
                ImGui::TextDisabled("No lineage data available");
            } else {
                ImGui::BeginChild("LineageStats", ImVec2(0, 120), true);
                std::istringstream lineageStream(lineageStats);
                std::string line;
                while (std::getline(lineageStream, line)) {
                    ImGui::TextWrapped("%s", line.c_str());
                }
                ImGui::EndChild();
            }
        }
        
        // Clear data button
        ImGui::Separator();
        if (ImGui::Button("Clear Diagnostic Data")) {
            cellManager.clearDiagnosticData();
        }
        addTooltip("Clear all recorded diagnostic data and reset counters");
        
        ImGui::Unindent();
    }
    
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
