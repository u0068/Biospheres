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

void UIManager::renderTimeScrubber(CellManager& cellManager, SceneManager& sceneManager)
{
    cellManager.setCellLimit(sceneManager.getCurrentCellLimit());
    // Set window size and position for a long horizontal resizable window
    ImGui::SetNextWindowPos(ImVec2(50, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 120), ImGuiCond_FirstUseEver);
    
    int flags = windowsLocked ? getWindowFlags(ImGuiWindowFlags_None) : getWindowFlags(ImGuiWindowFlags_None);
    if (ImGui::Begin("Time Scrubber", nullptr, flags))
    {
        // Update current simulation time from scene manager
        simulatedTime = sceneManager.getPreviewSimulationTime();
        
        // Get available width for responsive layout
        float available_width = ImGui::GetContentRegionAvail().x;
        
        // Title and main slider on one line
        ImGui::Text("Time Scrubber - Current Time: %.2fs", simulatedTime);
        
        // Show resimulation status indicator (fixed height to prevent layout shift)
        if (needsSimulationReset && isScrubbingTime) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); // Orange
            ImGui::Text("⚡ Resimulating...");
            ImGui::PopStyleColor();
        } else {
            // Reserve space to prevent layout shift
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(120.0f, ImGui::GetFrameHeight()));
        }
        
        // Calculate available width for slider (reserve space for input field)
        float input_width = 80.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float slider_width = available_width - input_width - spacing;
        
        // Update currentTime to match actual simulation time if not actively scrubbing
        if (!isScrubbingTime)
        {
            currentTime = simulatedTime;
            snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
        }
          // Make the slider take almost all available width
        ImGui::SetNextItemWidth(slider_width);
        
        // Check if slider is being actively dragged
        bool sliderChanged = ImGui::SliderFloat("##TimeSlider", &currentTime, 0.0f, maxTime, "%.2f");
        bool sliderActive = ImGui::IsItemActive(); // True while dragging
        
        if (sliderChanged)
        {
            // Update input buffer when slider changes
            snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
            targetTime = currentTime;
            
            // Mark as scrubbing
            isScrubbingTime = true;
            
            // Pause simulation while actively dragging
            if (sliderActive)
            {
                sceneManager.setPaused(true);
                
                // If keyframes are available, do REAL-TIME scrubbing (update immediately)
                if (keyframesInitialized)
                {
                    needsSimulationReset = true; // Trigger immediate update
                }
                else
                {
                    needsSimulationReset = false; // Wait for release if no keyframes
                }
            }
        }
        
        // Trigger resimulation when slider is released (if not already done)
        if (!sliderActive && isScrubbingTime && !keyframesInitialized)
        {
            // Slider was just released and no keyframes, trigger resimulation now
            needsSimulationReset = true;
        }
        
        // Draw hover marker on the slider
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 slider_min = ImGui::GetItemRectMin();
            ImVec2 slider_max = ImGui::GetItemRectMax();
            
            // Draw hover marker with time reference
            if (ImGui::IsItemHovered())
            {
                ImVec2 mouse_pos = ImGui::GetMousePos();
                
                // Calculate the time at mouse position
                float slider_width = slider_max.x - slider_min.x;
                float mouse_x = mouse_pos.x - slider_min.x;
                float hover_ratio = std::max(0.0f, std::min(1.0f, mouse_x / slider_width));
                float hover_time = hover_ratio * maxTime;
                
                // Draw vertical line at mouse position
                ImU32 marker_color = IM_COL32(0, 255, 255, 200); // Cyan
                float marker_x = slider_min.x + hover_ratio * slider_width;
                draw_list->AddLine(
                    ImVec2(marker_x, slider_min.y - 5.0f), 
                    ImVec2(marker_x, slider_max.y + 5.0f), 
                    marker_color, 
                    3.0f
                );
                
                // Draw time reference tooltip
                ImGui::BeginTooltip();
                ImGui::Text("Time: %.3f s", hover_time);
                
                // Show nearest keyframe info if available
                if (keyframesInitialized)
                {
                    int nearestIdx = findNearestKeyframe(hover_time);
                    if (nearestIdx >= 0 && nearestIdx < keyframes.size() && keyframes[nearestIdx].isValid)
                    {
                        float keyframe_time = keyframes[nearestIdx].time;
                        float time_diff = hover_time - keyframe_time;
                        ImGui::Separator();
                        ImGui::Text("Nearest keyframe: %.2f s", keyframe_time);
                        if (time_diff >= 0.0f) {
                            ImGui::Text("  +%.3f s from keyframe", time_diff);
                        } else {
                            ImGui::Text("  %.3f s before keyframe", -time_diff);
                        }
                    }
                }
                ImGui::EndTooltip();
            }
        }
        
        // Time input and controls on the same line
        ImGui::SameLine();
        ImGui::SetNextItemWidth(input_width);
        if (ImGui::InputText("##TimeInput", timeInputBuffer, sizeof(timeInputBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
        {
            // Parse input and update current time
            float inputTime = (float)atof(timeInputBuffer);
            if (inputTime >= 0.0f && inputTime <= maxTime)
            {
                currentTime = inputTime;
                targetTime = currentTime;
                needsSimulationReset = true;
                isScrubbingTime = true;
            }
            else
            {
                // Reset to current time if input is invalid
                snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
            }
        }
          // Max time control on a separate line but compact
        ImGui::Text("Max Time:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        float oldMaxTime = maxTime;
        if (ImGui::DragFloat("##MaxTime", &maxTime, 1.0f, 1.0f, 10000.0f, "%.2f"))
        {
            // Ensure current time doesn't exceed max time
            if (currentTime > maxTime)
            {
                currentTime = maxTime;
                snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
            }
            
            // If max time changed significantly, update keyframes
            if (abs(maxTime - oldMaxTime) > 0.1f)
            {
                updateKeyframes(cellManager, maxTime);
            }
        }
        
        // Keyframe initialization button and status
        ImGui::SameLine();
        if (ImGui::Button("Rebuild Keyframes"))
        {
            initializeKeyframes(cellManager);
        }
        
        // Show keyframe status
        ImGui::Text("Keyframes: %s (%d/%d)", 
                   keyframesInitialized ? "Ready" : "Not Ready", 
                   keyframesInitialized ? MAX_KEYFRAMES : 0,
                   MAX_KEYFRAMES);
        
        // Initialize keyframes if not done yet
        if (!keyframesInitialized)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Click 'Rebuild Keyframes' to enable real-time scrubbing");
        }
        else
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Real-time scrubbing enabled");
        }        // Handle time scrubbing
        if (needsSimulationReset && isScrubbingTime)
        {
            if (keyframesInitialized)
            {
                // Use keyframe system for efficient scrubbing
                int nearestKeyframeIndex = findNearestKeyframe(targetTime);
                const SimulationKeyframe& nearestKeyframe = keyframes[nearestKeyframeIndex];
                

                
                // Restore from nearest keyframe
                restoreFromKeyframe(cellManager, nearestKeyframeIndex);
                
                // Reset scene manager time to keyframe time
                sceneManager.resetPreviewSimulationTime();
                sceneManager.setPreviewSimulationTime(nearestKeyframe.time); // If target time is after the keyframe, simulate forward
                if (targetTime > nearestKeyframe.time)
                {
                    // Temporarily pause to prevent normal time updates during fast-forward
                    bool wasPaused = sceneManager.isPaused();
                    sceneManager.setPaused(true);
                    
                    float timeToSimulate = targetTime - nearestKeyframe.time;
                    
                    // Use optimized frame-skipping resimulation
                    int framesSkipped = cellManager.updateCellsFastForwardOptimized(
                        timeToSimulate, 
                        config::resimulationTimeStep
                    );
                    
                    // Update simulation time to target
                    sceneManager.setPreviewSimulationTime(targetTime);
                    
                    // Restore original pause state after fast-forward
                    sceneManager.setPaused(wasPaused);
                }
            }
            else
            {
                // Fallback to old method if keyframes not available
                
                // Reset the simulation
                cellManager.resetSimulation();
                cellManager.addGenomeToBuffer(currentGenome);
                ComputeCell newCell{};
                newCell.modeIndex = currentGenome.initialMode;
                cellManager.addCellToStagingBuffer(newCell);
                cellManager.addStagedCellsToQueueBuffer(); // Force immediate GPU buffer sync
                
                // Reset simulation time
                sceneManager.resetPreviewSimulationTime();
                  // If target time is greater than 0, fast-forward to that time
                if (targetTime > 0.0f)
                {
                    // Temporarily pause to prevent normal time updates during fast-forward
                    bool wasPaused = sceneManager.isPaused();
                    sceneManager.setPaused(true);
                    
                    // Use optimized frame-skipping resimulation
                    int framesSkipped = cellManager.updateCellsFastForwardOptimized(
                        targetTime, 
                        config::resimulationTimeStep
                    );
                    
                    // Update simulation time to target
                    sceneManager.setPreviewSimulationTime(targetTime);
                    
                    // Restore original pause state after fast-forward
                    sceneManager.setPaused(wasPaused);
                }
            }
            
            needsSimulationReset = false;
            
            // Only mark scrubbing as complete if slider is not active (released)
            // This allows real-time scrubbing to continue while dragging
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                isScrubbingTime = false;
            }
            
            // Keep simulation paused after scrubbing to prevent immediate drift
            sceneManager.setPaused(true);
        }
    }
    
    ImGui::End();
}