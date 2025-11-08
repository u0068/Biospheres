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
#include <stdexcept>

#include "../audio/audio_engine.h"
#include "../scene/scene_manager.h"
#include "../simulation/cpu_preview/cpu_preview_system.h"
#include "../simulation/cpu_preview/cpu_genome_converter.h"

// Ensure std::min and std::max are available
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

void UIManager::renderTimeScrubber(CellManager& cellManager, SceneManager& sceneManager, CPUPreviewSystem* cpuPreviewSystem)
{
    cellManager.setCellLimit(sceneManager.getCurrentCellLimit());
    // Set window size and position for a long horizontal resizable window
    ImGui::SetNextWindowPos(ImVec2(1398, 15), ImGuiCond_FirstUseEver);
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
            float previousTime = sceneManager.getPreviewSimulationTime();
            
            // Update input buffer when slider changes
            snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
            targetTime = currentTime;
            
            // Mark as scrubbing - preview simulation should always be paused
            isScrubbingTime = true;
            
            // Ensure simulation stays paused (preview simulation is always manual)
            sceneManager.setPaused(true);
            
            // Only trigger resimulation if moving backward
            // For forward movements, just continue simulating from current state
            if (currentTime < previousTime) {
                needsSimulationReset = true;
            } else {
                // Moving forward - just continue simulation
                needsSimulationReset = false;
            }
        }
        
        // No need for release-based triggering since we trigger on every movement
        
        // Handle completion of text input scrubbing
        static bool wasTextInputActive = false;
        bool isTextInputActive = ImGui::IsItemActive();
        if (wasTextInputActive && !isTextInputActive && isScrubbingTime && !needsSimulationReset)
        {
            // Text input was just completed, keep simulation paused
            isScrubbingTime = false;
            sceneManager.setPaused(true);
        }
        wasTextInputActive = isTextInputActive;
        
        // Draw hover marker on the slider
        {
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImVec2 slider_min = ImGui::GetItemRectMin();
            ImVec2 slider_max = ImGui::GetItemRectMax();

            // Draw hover marker with time reference
            if (ImGui::IsItemHovered())
            {
                ImVec2 mouse_pos = ImGui::GetMousePos();

                // Account for ImGui's internal padding in SliderFloat
                // ImGui uses grab_padding which is typically style.GrabMinSize * 0.5f
                const float grab_padding = ImGui::GetStyle().GrabMinSize * 0.5f;
                const float slider_usable_sz = (slider_max.x - slider_min.x) - grab_padding * 2.0f;
                const float slider_usable_pos_min = slider_min.x + grab_padding;

                // Calculate the time at mouse position using ImGui's internal logic
                float mouse_abs_pos = mouse_pos.x;
                float clicked_t = (slider_usable_sz > 0.0f) ? std::max(0.0f, std::min(1.0f, (mouse_abs_pos - slider_usable_pos_min) / slider_usable_sz)) : 0.0f;
                float hover_time = 0.0f + clicked_t * (maxTime - 0.0f);

                // Calculate marker position using the same ratio
                float marker_x = slider_usable_pos_min + clicked_t * slider_usable_sz;

                // Draw vertical line at mouse position
                ImU32 marker_color = IM_COL32(0, 255, 255, 200); // Cyan
                draw_list->AddLine(
                    ImVec2(marker_x, slider_min.y - 5.0f),
                    ImVec2(marker_x, slider_max.y + 5.0f),
                    marker_color,
                    3.0f
                );

                // Draw time reference tooltip
                ImGui::BeginTooltip();
                ImGui::Text("Time: %.2f s", hover_time);
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
                
                // Ensure simulation stays paused (preview simulation is always manual)
                sceneManager.setPaused(true);
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

        // Handle time scrubbing - always do complete resimulation from scratch
        if (needsSimulationReset && isScrubbingTime)
        {
            if (cpuPreviewSystem)
            {
                // Always do complete resimulation from time 0 to target time using CPU Preview System
                
                try {
                    // Reset the CPU Preview System completely
                    cpuPreviewSystem->reset();
                    
                    // Add default cell with current genome
                    CPUCellParameters defaultCell;
                    defaultCell.position = glm::vec3(0.0f, 0.0f, 0.0f);
                    defaultCell.velocity = glm::vec3(0.0f, 0.0f, 0.0f);
                    defaultCell.orientation = glm::quat(
                        currentGenome.initialOrientation.w,
                        currentGenome.initialOrientation.x,
                        currentGenome.initialOrientation.y,
                        currentGenome.initialOrientation.z
                    );
                    defaultCell.mass = 1.0f;
                    defaultCell.radius = 1.0f;
                    defaultCell.cellType = 0;
                    defaultCell.genomeID = 0;
                    defaultCell.genome = CPUGenomeConverter::convertToCPUFormat(currentGenome);
                    
                    cpuPreviewSystem->addCell(defaultCell);
                    
                    // Reset simulation time to 0
                    sceneManager.resetPreviewSimulationTime();
                    
                    // If target time is greater than 0, simulate from 0 to target time
                    if (targetTime > 0.0f)
                    {
                        // Ensure simulation stays paused during simulation
                        sceneManager.setPaused(true);
                        
                        // Use optimized fast-forward method - suppresses visual updates during simulation
                        // to prevent flashing and ghost cells, only updates visuals at the end
                        cpuPreviewSystem->fastForward(targetTime, config::resimulationTimeStep);
                        
                        // Update simulation time to target
                        sceneManager.setPreviewSimulationTime(targetTime);
                    }
                    
                    // Visual data will be updated when needed for rendering
                    
                } catch (const std::exception& e) {
                    std::cerr << "Error during time scrubber resimulation: " << e.what() << std::endl;
                }
            } else {
                std::cerr << "CPU Preview System not available for time scrubbing" << std::endl;
            }
            
            needsSimulationReset = false;
            
            // Only mark scrubbing as complete if slider is not active (released)
            // This allows real-time scrubbing to continue while dragging
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                isScrubbingTime = false;
                // Keep simulation paused - preview simulation is always manual
                sceneManager.setPaused(true);
            }
        }
        
        // Handle forward-only simulation (no reset needed)
        if (isScrubbingTime && !needsSimulationReset && cpuPreviewSystem)
        {
            float previousTime = sceneManager.getPreviewSimulationTime();
            float deltaTime = targetTime - previousTime;
            
            if (deltaTime > 0.0f)
            {
                try {
                    // Just simulate forward from current state
                    cpuPreviewSystem->fastForward(deltaTime, config::resimulationTimeStep);
                    sceneManager.setPreviewSimulationTime(targetTime);
                } catch (const std::exception& e) {
                    std::cerr << "Error during forward simulation: " << e.what() << std::endl;
                }
            }
            
            // Mark scrubbing as complete if slider is not active
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                isScrubbingTime = false;
                sceneManager.setPaused(true);
            }
        }
    }
    
    ImGui::End();
}