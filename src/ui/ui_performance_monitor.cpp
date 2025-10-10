#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../core/config.h"
#include "imgui.h"
#include "ui_layout.h"
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

void UIManager::renderPerformanceMonitor(CellManager &cellManager, PerformanceMonitor &perfMonitor, SceneManager& sceneManager)
{
    cellManager.setCellLimit(sceneManager.getCurrentCellLimit());
    cellManager.updateCounts();
    ImGui::SetNextWindowPos(UILayout::Layout::getPerformanceMonitorPos(), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(UILayout::Layout::getPerformanceMonitorSize(), ImGuiCond_FirstUseEver);
	int flags = getWindowFlags();
    if (ImGui::Begin("Advanced Performance Monitor", nullptr, flags))
    {

    // === FPS and Frame Time Section ===
    ImGui::Text("Performance Overview");
    ImGui::Separator();

    // Main metrics with color coding
    ImGui::Text("FPS: ");
    ImGui::SameLine();
    ImVec4 fpsColor = perfMonitor.displayFPS >= 59.0f ? ImVec4(0, 1, 0, 1) : perfMonitor.displayFPS >= 30.0f ? ImVec4(1, 1, 0, 1)
                                                                                                             : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(fpsColor, "%.1f", perfMonitor.displayFPS);

    ImGui::Text("Frame Time: ");
    ImGui::SameLine();
    ImVec4 frameTimeColor = perfMonitor.displayFrameTime <= 17.f ? ImVec4(0, 1, 0, 1) : perfMonitor.displayFrameTime <= 33.33f ? ImVec4(1, 1, 0, 1)
                                                                                                                                 : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(frameTimeColor, "%.3f ms", perfMonitor.displayFrameTime);

    // Frame time statistics
    ImGui::Text("Min/Avg/Max: %.2f/%.2f/%.2f ms",
                perfMonitor.minFrameTime, perfMonitor.avgFrameTime, perfMonitor.maxFrameTime);

    // === Performance Graphs ===
    ImGui::Spacing();
    ImGui::Text("Frame Time History");
    if (!perfMonitor.frameTimeHistory.empty())
    {
        ImGui::PlotLines("##FrameTime", perfMonitor.frameTimeHistory.data(),
                         static_cast<int>(perfMonitor.frameTimeHistory.size()), 0, nullptr,
                         0.0f, 50.0f, ImVec2(0, 80));
    }

    ImGui::Text("FPS History");
    if (!perfMonitor.fpsHistory.empty())
    {
        ImGui::PlotLines("##FPS", perfMonitor.fpsHistory.data(),
                         static_cast<int>(perfMonitor.fpsHistory.size()), 0, nullptr,
                         0.0f, 120.0f, ImVec2(0, 80));
    } // === Performance Bars ===
    ImGui::Spacing();
    ImGui::Text("Performance Indicators");
    ImGui::Separator();

    // Advanced FPS performance visualization
    float fpsRatio = std::min(perfMonitor.displayFPS / 120.0f, 1.0f);

    // Color-coded FPS performance bar
    ImVec4 fpsBarColor;
    std::string fpsStatus;
    if (perfMonitor.displayFPS >= 59.0f)
    {
        fpsBarColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
        fpsStatus = "Excellent";
    }
    else if (perfMonitor.displayFPS >= 45.0f)
    {
        fpsBarColor = ImVec4(0.5f, 0.8f, 0.0f, 1.0f); // Yellow-Green
        fpsStatus = "Good";
    }
    else if (perfMonitor.displayFPS >= 30.0f)
    {
        fpsBarColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Orange
        fpsStatus = "Fair";
    }
    else
    {
        fpsBarColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
        fpsStatus = "Poor";
    }

    ImGui::Text("FPS Performance: %.1f (%s)", perfMonitor.displayFPS, fpsStatus.c_str());
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, fpsBarColor);
    ImGui::ProgressBar(fpsRatio, ImVec2(-1, 25), ""); // Wider bar
    ImGui::PopStyleColor();

    // FPS target indicators
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

    // 60 FPS indicator
    if (perfMonitor.displayFPS >= 59.0f)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "60+");
    else
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "60");

    ImGui::SameLine();
    // 30 FPS indicator
    if (perfMonitor.displayFPS >= 30.0f)
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "30+");
    else
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "30");

    ImGui::PopStyleVar();
    ImGui::EndGroup();

    // Frame time performance bar with enhanced visuals
    float frameTimeRatio = 1.0f - std::min(perfMonitor.displayFrameTime / 50.0f, 1.0f);
    if (frameTimeRatio < 0)
        frameTimeRatio = 0;

    ImVec4 frameTimeBarColor;
    std::string frameTimeStatus;
    if (perfMonitor.displayFrameTime <= 17.f)
    {
        frameTimeBarColor = ImVec4(0.0f, 0.8f, 0.0f, 1.0f); // Green
        frameTimeStatus = "Smooth";
    }
    else if (perfMonitor.displayFrameTime <= 25.0f)
    {
        frameTimeBarColor = ImVec4(0.5f, 0.8f, 0.0f, 1.0f); // Yellow-Green
        frameTimeStatus = "Good";
    }
    else if (perfMonitor.displayFrameTime <= 33.33f)
    {
        frameTimeBarColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f); // Orange
        frameTimeStatus = "Acceptable";
    }
    else
    {
        frameTimeBarColor = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red
        frameTimeStatus = "Laggy";
    }

    ImGui::Text("Frame Time: %.2f ms (%s)", perfMonitor.displayFrameTime, frameTimeStatus.c_str());
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, frameTimeBarColor);
    ImGui::ProgressBar(frameTimeRatio, ImVec2(-1, 25), ""); // Wider bar
    ImGui::PopStyleColor();

    // === System Information ===
    ImGui::Spacing();
    ImGui::Text("System Information");
    ImGui::Separator();

    // GPU Information
    const char *renderer = (const char *)glGetString(GL_RENDERER);
    const char *vendor = (const char *)glGetString(GL_VENDOR);
    const char *version = (const char *)glGetString(GL_VERSION);

    if (renderer)
        ImGui::Text("GPU: %s", renderer);
    if (vendor)
        ImGui::Text("Vendor: %s", vendor);
    if (version)
        ImGui::Text("OpenGL: %s", version);

    // === Simulation Metrics ===
    ImGui::Spacing();
    ImGui::Text("Simulation Metrics");
    ImGui::Separator();

    int cellCount = cellManager.getCellCount();
    ImGui::Text("Cells: %i / %i / %i", cellManager.liveCellCount, cellManager.totalCellCount, cellManager.cellLimit);
    ImGui::Text("Adhesion Connections: %i / %i / %i", cellManager.liveAdhesionCount, cellManager.totalAdhesionCount, cellManager.getAdhesionLimit());
    ImGui::Text("Pending Cells: %i", cellManager.pendingCellCount);
    ImGui::Text("Triangles: %i", cellManager.getTotalTriangleCount());
    ImGui::Text("Vertices: %i", cellManager.getTotalVertexCount());

    // Memory estimate
    float memoryMB = (cellCount * sizeof(ComputeCell)) / (1024.0f * 1024.0f);
    ImGui::Text("Cell Data Memory: %.2f MB", memoryMB);

    // === Performance Warnings ===
    ImGui::Spacing();
    if (perfMonitor.displayFPS < 30.0f)
    {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "⚠ Low FPS detected!");
        ImGui::TextWrapped("Performance is below 30 FPS. Consider reducing cell count or adjusting quality settings.");
    }

    if (perfMonitor.displayFrameTime > 33.33f)
    {
        ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "⚠ High frame time!");
        ImGui::TextWrapped("Frame time is over 33ms. This may cause stuttering.");
    }

    // === Debug Information ===
    if (ImGui::CollapsingHeader("Debug Information"))
    {
        ImGui::Text("Frame Count: %d", perfMonitor.frameCount);
        ImGui::Text("Update Interval: %.3f s", perfMonitor.perfUpdateInterval);
        ImGui::Text("Last Update: %.3f s ago", perfMonitor.lastPerfUpdate);
        ImGui::Text("History Size: %zu entries", perfMonitor.frameTimeHistory.size());
        
        // LOD distribution information
        if (ImGui::CollapsingHeader("LOD Distribution"))
        {
            const int* lodCounts = cellManager.lodInstanceCounts;
            ImGui::Text("LOD 0 (32x32): %d cells", lodCounts[0]);
            ImGui::Text("LOD 1 (16x16): %d cells", lodCounts[1]);
            ImGui::Text("LOD 2 (8x8):   %d cells", lodCounts[2]);
            ImGui::Text("LOD 3 (4x4):   %d cells", lodCounts[3]);
            
            int totalLodCells = lodCounts[0] + lodCounts[1] + lodCounts[2] + lodCounts[3];
            if (totalLodCells > 0) {
                ImGui::Text("LOD Coverage: %d / %d cells (%.1f%%)", 
                    totalLodCells, cellCount, 
                    (float)totalLodCells / cellCount * 100.0f);
            }
        }
        
        // Frustum culling information
        if (ImGui::CollapsingHeader("Frustum Culling"))
        {
            ImGui::Text("Enabled: %s", cellManager.useFrustumCulling ? "Yes" : "No");
            if (cellManager.useFrustumCulling) {
                int visibleCells = cellManager.getVisibleCellCount();
                ImGui::Text("Visible Cells: %d / %d", visibleCells, cellCount);
                if (cellCount > 0) {
                    float cullingRatio = (float)(cellCount - visibleCells) / cellCount * 100.0f;
                    ImGui::Text("Culled: %.1f%%", cullingRatio);
                }
            }
        }
        
        // Distance-based culling information
        if (ImGui::CollapsingHeader("Distance Culling & Fading"))
        {
            ImGui::Text("Enabled: %s", cellManager.useDistanceCulling ? "Yes" : "No");
            if (cellManager.useDistanceCulling) {
                int visibleCells = cellManager.getVisibleCellCount();
                ImGui::Text("Visible Cells: %d / %d", visibleCells, cellCount);
                if (cellCount > 0) {
                    float cullingRatio = (float)(cellCount - visibleCells) / cellCount * 100.0f;
                    ImGui::Text("Culled: %.1f%%", cullingRatio);
                }
                ImGui::Text("Max Distance: %.0f", cellManager.getMaxRenderDistance());
                ImGui::Text("Fade Start: %.0f", cellManager.getFadeStartDistance());
                ImGui::Text("Fade End: %.0f", cellManager.getFadeEndDistance());
            }
        }

        //// Readback system status if available
        //if (cellManager.isReadbackSystemHealthy())
        //{
        //    ImGui::TextColored(ImVec4(0, 1, 0, 1), "✓ GPU Readback: Healthy");
        //    if (cellManager.isReadbackInProgress())
        //    {
        //        ImGui::Text("  Readback in progress...");
        //    }        //    ImGui::Text("  Cooldown: %.2f s", cellManager.getReadbackCooldown());
        //}
        //else
        //{
        //    ImGui::TextColored(ImVec4(1, 0, 0, 1), "✗ GPU Readback: Unavailable");
        //}
    }

    }
    ImGui::End();
}