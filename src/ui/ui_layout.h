#pragma once
#include "../../third_party/imgui/imgui.h"
#include <algorithm>
#include <cmath>

// Undefine Windows min/max macros
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// UI Layout Manager
// Provides dynamic window positioning based on viewport size
namespace UILayout
{
    // Store viewport dimensions
    struct ViewportInfo
    {
        float width = 1920.0f;
        float height = 1080.0f;
        float previousWidth = 1920.0f;
        float previousHeight = 1080.0f;
        bool initialized = false;
        bool wasResized = false;
    };

    inline ViewportInfo viewportInfo;

    // Update viewport dimensions (call once per frame)
    inline void updateViewport()
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        
        // Check if viewport was resized
        float newWidth = viewport->Size.x;
        float newHeight = viewport->Size.y;
        
        if (viewportInfo.initialized)
        {
            // Detect resize with a small threshold to avoid floating point issues
            const float threshold = 1.0f;
            if (std::abs(newWidth - viewportInfo.width) > threshold || 
                std::abs(newHeight - viewportInfo.height) > threshold)
            {
                viewportInfo.previousWidth = viewportInfo.width;
                viewportInfo.previousHeight = viewportInfo.height;
                viewportInfo.wasResized = true;
            }
            else
            {
                viewportInfo.wasResized = false;
            }
        }
        
        viewportInfo.width = newWidth;
        viewportInfo.height = newHeight;
        viewportInfo.initialized = true;
    }

    // Clamp window position to keep it within viewport bounds
    inline ImVec2 clampToViewport(const ImVec2& pos, const ImVec2& size)
    {
        ImVec2 clampedPos = pos;
        
        // Clamp X position
        if (clampedPos.x < 0.0f)
            clampedPos.x = 0.0f;
        if (clampedPos.x + size.x > viewportInfo.width)
            clampedPos.x = viewportInfo.width - size.x;
        
        // Clamp Y position
        if (clampedPos.y < 0.0f)
            clampedPos.y = 0.0f;
        if (clampedPos.y + size.y > viewportInfo.height)
            clampedPos.y = viewportInfo.height - size.y;
        
        return clampedPos;
    }


    // Apply window constraints to keep windows within viewport
    inline void applyWindowConstraints()
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            // If multi-viewport is enabled, don't constrain
            return;
        }
        
        // Set constraint for all windows to stay within main viewport
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowViewport(viewport->ID);
    }

    // Helper to calculate percentage-based positions
    inline ImVec2 getPosition(float xPercent, float yPercent)
    {
        return ImVec2(
            viewportInfo.width * xPercent,
            viewportInfo.height * yPercent
        );
    }

    // Helper to calculate percentage-based sizes
    inline ImVec2 getSize(float widthPercent, float heightPercent)
    {
        return ImVec2(
            viewportInfo.width * widthPercent,
            viewportInfo.height * heightPercent
        );
    }

    // Setup default docking layout (call once when initializing or resetting layout)
    inline void setupDefaultDockLayout()
    {
        // Note: This requires ImGui docking branch
        // The docking layout will be created from the imgui.ini file
        // Users can customize and save their preferred layout
        // This function is here for future expansion if needed
    }

    // Predefined layout positions for different windows
    // Adaptive layout that works on both ultrawide (3440x1440) and standard (1920x1080) resolutions
    namespace Layout
    {
        // Genome Editor - Top Left
        inline ImVec2 getGenomeEditorPos() { return ImVec2(10.0f, 15.0f); }
        inline ImVec2 getGenomeEditorSize() { return ImVec2(800.0f, 600.0f); }

        // Time Scrubber - Top Center (adapts based on width)
        inline ImVec2 getTimeScrubberPos() { 
            float centerX = (viewportInfo.width - 800.0f) * 0.5f;
            return ImVec2(std::max(820.0f, centerX), 15.0f); 
        }
        inline ImVec2 getTimeScrubberSize() { return ImVec2(800.0f, 120.0f); }

        // Cell Inspector - Bottom Left (docked with Performance Monitor below it)
        inline ImVec2 getCellInspectorPos() { return ImVec2(6.0f, viewportInfo.height - 692.0f); }
        inline ImVec2 getCellInspectorSize() { return ImVec2(388.0f, 347.0f); }

        // Performance Monitor - Below Cell Inspector (docked together)
        inline ImVec2 getPerformanceMonitorPos() { return ImVec2(6.0f, viewportInfo.height - 343.0f); }
        inline ImVec2 getPerformanceMonitorSize() { return ImVec2(388.0f, 343.0f); }

        // Scene Switcher - Right side (adapts to screen width)
        inline ImVec2 getSceneSwitcherPos() { 
            return ImVec2(viewportInfo.width - 330.0f, 50.0f); 
        }
        inline ImVec2 getSceneSwitcherSize() { return ImVec2(320.0f, 413.0f); }

        // Camera Controls - Right side below Scene Switcher
        inline ImVec2 getCameraControlsPos() { 
            return ImVec2(viewportInfo.width - 330.0f, 475.0f); 
        }
        inline ImVec2 getCameraControlsSize() { return ImVec2(320.0f, 562.0f); }

        // Simulation Settings - Below Cell Inspector
        inline ImVec2 getSimulationSettingsPos() { 
            return ImVec2(10.0f, viewportInfo.height - 750.0f); 
        }
        inline ImVec2 getSimulationSettingsSize() { return ImVec2(388.0f, 380.0f); }
    }
}
