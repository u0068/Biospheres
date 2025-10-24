#include "ui_manager.h"
#include "../rendering/systems/bridge_rendering_system.h"
#include "../core/config.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <iostream>

// Ensure std::min and std::max are available
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// ============================================================================
// BRIDGE RENDERING CONTROLS SECTION (Requirement 11.1, 16.1)
// ============================================================================

void UIManager::renderBridgeRenderingControls(BridgeRenderingSystem& bridgeSystem)
{
    ImGui::SetNextWindowPos(ImVec2(815, 621), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    
    if (ImGui::Begin("Bridge Rendering System", nullptr, flags))
    {
        // === System Status Section ===
        ImGui::Text("System Status");
        ImGui::Separator();
        
        // Enable/Disable toggle (Requirement 11.1)
        bool enabled = bridgeSystem.isEnabled();
        if (ImGui::Checkbox("Enable Bridge Rendering", &enabled))
        {
            bridgeSystem.setEnabled(enabled);
        }
        
        // System status indicator
        if (enabled)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "● Active");
        }
        else
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "○ Disabled");
        }
        
        ImGui::Spacing();
        
        // === Performance Statistics Section (Requirement 16.1) ===
        ImGui::Text("Performance Statistics");
        ImGui::Separator();
        
        // Instance count
        int instanceCount = bridgeSystem.getInstanceCount();
        ImGui::Text("Bridge Instances: %d", instanceCount);
        
        // Frame time breakdown
        float frameTime = bridgeSystem.getLastFrameTime();
        float computeTime = bridgeSystem.getLastComputeTime();
        float renderTime = bridgeSystem.getLastRenderTime();
        
        ImGui::Text("Total Frame Time: %.3f ms", frameTime);
        ImGui::Text("  Compute Time: %.3f ms", computeTime);
        ImGui::Text("  Render Time: %.3f ms", renderTime);
        
        // Calculate FPS from frame time
        float fps = frameTime > 0.0f ? 1000.0f / frameTime : 0.0f;
        ImGui::Text("Rendering FPS: %.1f", fps);
        
        // Performance warning (Requirement 16.1)
        if (fps > 0.0f && fps < 30.0f)
        {
            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "⚠ Low FPS Warning!");
            ImGui::TextWrapped("Bridge rendering FPS is below 30. Consider reducing instance count or disabling the system.");
        }
        
        // Performance bar visualization
        ImGui::Spacing();
        ImGui::Text("Performance:");
        
        float fpsRatio = std::min(fps / 60.0f, 1.0f);
        ImVec4 fpsBarColor;
        if (fps >= 59.0f)
            fpsBarColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
        else if (fps >= 30.0f)
            fpsBarColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Orange
        else
            fpsBarColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fpsBarColor);
        ImGui::ProgressBar(fpsRatio, ImVec2(-1, 20), "");
        ImGui::PopStyleColor();
        
        // Timing breakdown visualization
        if (frameTime > 0.0f)
        {
            ImGui::Text("Time Breakdown:");
            float computeRatio = computeTime / frameTime;
            float renderRatio = renderTime / frameTime;
            
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.6f, 1.0f, 1.0f)); // Blue for compute
            ImGui::ProgressBar(computeRatio, ImVec2(-1, 15), "Compute");
            ImGui::PopStyleColor();
            
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.6f, 0.2f, 1.0f, 1.0f)); // Purple for render
            ImGui::ProgressBar(renderRatio, ImVec2(-1, 15), "Render");
            ImGui::PopStyleColor();
        }
        
        ImGui::Spacing();
        
        // === Mesh Library Section ===
        ImGui::Text("Mesh Library");
        ImGui::Separator();
        
        // Reload mesh library button
        if (ImGui::Button("Reload Mesh Library", ImVec2(-1, 0)))
        {
            std::string meshDirectory = "assets/meshes/bridges/";
            bridgeSystem.loadBakedMeshes(meshDirectory);
        }
        
        ImGui::Spacing();
        
        // === Debug Visualization Section ===
        if (ImGui::CollapsingHeader("Debug Visualization"))
        {
            ImGui::Text("Debug options will be added in future tasks");
            ImGui::Spacing();
            
            // Placeholder for debug visualization options
            static bool showBridgeWireframe = false;
            ImGui::Checkbox("Show Bridge Wireframe", &showBridgeWireframe);
            
            static bool showInstanceBounds = false;
            ImGui::Checkbox("Show Instance Bounds", &showInstanceBounds);
            
            static bool colorByParameter = false;
            ImGui::Checkbox("Color by Parameter", &colorByParameter);
            
            ImGui::Spacing();
            ImGui::TextDisabled("(Debug features not yet implemented)");
        }
        
        // === Advanced Settings Section ===
        if (ImGui::CollapsingHeader("Advanced Settings"))
        {
            ImGui::Text("Advanced configuration options");
            ImGui::Spacing();
            
            // Placeholder for advanced settings
            static int maxInstances = 100000;
            ImGui::SliderInt("Max Instances", &maxInstances, 1000, 200000);
            
            static float interpolationQuality = 1.0f;
            ImGui::SliderFloat("Interpolation Quality", &interpolationQuality, 0.0f, 1.0f);
            
            ImGui::Spacing();
            ImGui::TextDisabled("(Settings not yet connected to system)");
        }
        
        // === Information Section ===
        if (ImGui::CollapsingHeader("Information"))
        {
            ImGui::TextWrapped("The Bridge Rendering System renders organic connections between adhesion-connected cells using pre-baked metaball mesh variations.");
            ImGui::Spacing();
            ImGui::Text("Features:");
            ImGui::BulletText("GPU-driven instance generation");
            ImGui::BulletText("Bilinear mesh interpolation");
            ImGui::BulletText("Parameter-based mesh selection");
            ImGui::BulletText("Instanced rendering for performance");
        }
    }
    ImGui::End();
}
