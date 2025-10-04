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
        if (ImGui::SliderFloat("##TimeSlider", &currentTime, 0.0f, maxTime, "%.2f"))
        {
            // Update input buffer when slider changes
            snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
            targetTime = currentTime;
            needsSimulationReset = true;
            isScrubbingTime = true;
        }
        
        // Draw keyframe indicators on the slider
        if (keyframesInitialized)
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 slider_min = ImGui::GetItemRectMin();
            ImVec2 slider_max = ImGui::GetItemRectMax();
            
            // Draw keyframe markers
            for (int i = 0; i < MAX_KEYFRAMES && i < keyframes.size(); i++)
            {
                if (keyframes[i].isValid)
                {
                    float keyframe_time = keyframes[i].time;
                    float t = (keyframe_time / maxTime);
                    float x = slider_min.x + t * (slider_max.x - slider_min.x);
                    
                    // Draw a small vertical line to indicate keyframe position
                    ImU32 color = IM_COL32(255, 255, 0, 180); // Yellow with transparency
                    draw_list->AddLine(ImVec2(x, slider_min.y), ImVec2(x, slider_max.y), color, 2.0f);
                }
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
        ImGui::Text("Keyframes: %s (%d/50)", 
                   keyframesInitialized ? "Ready" : "Not Ready", 
                   keyframesInitialized ? MAX_KEYFRAMES : 0);
        
        // Initialize keyframes if not done yet
        if (!keyframesInitialized)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "Click 'Rebuild Keyframes' to enable efficient scrubbing");
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
                    
                    float timeRemaining = targetTime - nearestKeyframe.time;
                    float physicsTimeStep = config::physicsTimeStep;
                    int maxSteps = (int)(timeRemaining / physicsTimeStep) + 1;
                    

                    
                    for (int i = 0; i < maxSteps && timeRemaining > 0.0f; ++i)
                    {
                        float stepTime = (timeRemaining > physicsTimeStep) ? physicsTimeStep : timeRemaining;
                        cellManager.updateCellsFastForward(stepTime); // Use optimized fast-forward
                        timeRemaining -= stepTime;
                        
                        // Update simulation time manually during fast-forward
                        sceneManager.setPreviewSimulationTime(targetTime - timeRemaining);
                    }
                    
                    // CRITICAL FIX: Verify timing accuracy after fast-forward
                    if (nearestKeyframeIndex < keyframes.size() && keyframes[nearestKeyframeIndex].cellCount > 0) {
                        cellManager.syncCellPositionsFromGPU();
                        ComputeCell currentCell = cellManager.getCellData(0);
                        float expectedAge = keyframes[nearestKeyframeIndex].cellStates[0].age + (targetTime - nearestKeyframe.time);
                        float ageDiff = abs(currentCell.age - expectedAge);
                        
                        if (ageDiff > 0.01f) {
                            std::cout << "WARNING: Cell age timing drift detected after fast-forward!\n";
                            std::cout << "Expected age: " << expectedAge << ", Actual age: " << currentCell.age 
                                      << ", Difference: " << ageDiff << "s\n";
                        }
                    }
                    
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
                	// Use a coarser time step for scrubbing to make it more responsive
                    float scrubTimeStep = config::scrubTimeStep;
                    float timeRemaining = targetTime;
                    int maxSteps = (int)(targetTime / scrubTimeStep) + 1;
                    
                    for (int i = 0; i < maxSteps && timeRemaining > scrubTimeStep; ++i)
                    {
                        float stepTime = scrubTimeStep;
                        cellManager.updateCellsFastForward(stepTime); // Use optimized fast-forward
                        timeRemaining -= stepTime;
                        
                        // Update simulation time manually during fast-forward
                        sceneManager.setPreviewSimulationTime(targetTime - timeRemaining);
                    }
                    
                    // Restore original pause state after fast-forward
                    sceneManager.setPaused(wasPaused);
                }
            }
            
            needsSimulationReset = false;
            isScrubbingTime = false;
        }
    }
    ImGui::End();
}