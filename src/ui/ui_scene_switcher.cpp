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

void UIManager::renderSceneSwitcher(SceneManager& sceneManager, CellManager& previewCellManager, CellManager& mainCellManager)
{
    // Set window position on first use - top center
    ImGui::SetNextWindowPos(ImVec2(3072, 46), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 413), ImGuiCond_FirstUseEver);
    int flags = windowsLocked ? getWindowFlags(ImGuiWindowFlags_None) : getWindowFlags(ImGuiWindowFlags_None);
    if (ImGui::Begin("Scene Manager", nullptr, flags))
    {
        // Get current scene for use throughout the function
        Scene currentScene = sceneManager.getCurrentScene();
        
        // === CURRENT SCENE SECTION ===
        ImGui::Text("Current Scene: %s", sceneManager.getCurrentSceneName());
        ImGui::Separator();        // === SIMULATION CONTROLS SECTION ===
        // Only show simulation controls for Main Simulation
        // Preview Simulation uses Time Scrubber for time control
        if (currentScene == Scene::MainSimulation)
        {
            ImGui::Text("Simulation Controls");
            
            // Pause/Resume button
            bool isPaused = sceneManager.isPaused();
            if (isPaused)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f)); // Green for resume
                if (ImGui::Button("Resume Simulation", ImVec2(150, 30)))
                {
                    sceneManager.setPaused(false);
                }
                ImGui::PopStyleColor();
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.7f, 0.2f, 1.0f)); // Yellow for pause
                if (ImGui::Button("Pause Simulation", ImVec2(150, 30)))
                {
                    sceneManager.setPaused(true);
                }
                ImGui::PopStyleColor();
            }
            
            // Reset button next to pause/resume
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f)); // Red for reset
            if (ImGui::Button("Reset Main", ImVec2(150, 30)))
            {
                mainCellManager.resetSimulation();
                mainCellManager.addGenomeToBuffer(currentGenome);
                ComputeCell newCell{};
                newCell.modeIndex = currentGenome.initialMode;
                mainCellManager.addCellToStagingBuffer(newCell);
                mainCellManager.addStagedCellsToQueueBuffer(); // Force immediate GPU buffer sync
                
                // Advance simulation by one frame after reset
                mainCellManager.updateCells(config::physicsTimeStep);
            }
            ImGui::PopStyleColor();
        }
        else if (currentScene == Scene::PreviewSimulation)
        {
            ImGui::Text("Simulation Controls");
            ImGui::TextDisabled("Time control available in Time Scrubber window");
        }        
        // Speed controls - only show for Main Simulation
        if (currentScene == Scene::MainSimulation)
        {
            float currentSpeed = sceneManager.getSimulationSpeed();
            ImGui::Text("Speed: %.1fx", currentSpeed);
            
            // Speed slider
            if (ImGui::SliderFloat("##Speed", &currentSpeed, 0.1f, 10.0f, "%.1fx"))
            {
                sceneManager.setSimulationSpeed(currentSpeed);
            }
            
            // Quick speed buttons
            ImGui::Text("Quick Speed:");
            if (ImGui::Button("0.25x", ImVec2(50, 25))) sceneManager.setSimulationSpeed(0.25f);
            ImGui::SameLine();
            if (ImGui::Button("0.5x", ImVec2(50, 25))) sceneManager.setSimulationSpeed(0.5f);
            ImGui::SameLine();
            if (ImGui::Button("1x", ImVec2(50, 25))) sceneManager.setSimulationSpeed(1.0f);
            ImGui::SameLine();
            if (ImGui::Button("2x", ImVec2(50, 25))) sceneManager.setSimulationSpeed(2.0f);
            ImGui::SameLine();
            if (ImGui::Button("5x", ImVec2(50, 25))) sceneManager.setSimulationSpeed(5.0f);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
          // === SCENE SWITCHING SECTION ===
        ImGui::Text("Scene Switching");
        
        if (currentScene == Scene::PreviewSimulation)
        {
            if (ImGui::Button("Switch to Main Simulation", ImVec2(200, 30)))
            {
                sceneManager.switchToScene(Scene::MainSimulation);
            }
        }
        else if (currentScene == Scene::MainSimulation)
        {
            if (ImGui::Button("Switch to Preview Simulation", ImVec2(200, 30)))
            {
                sceneManager.switchToScene(Scene::PreviewSimulation);
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();        // Status info
        ImGui::Spacing();
        if (currentScene == Scene::PreviewSimulation)
        {
            ImGui::TextDisabled("Time: %.2fs (controlled by Time Scrubber)", 
                sceneManager.getPreviewSimulationTime());
        }
        else
        {
            bool isPaused = sceneManager.isPaused();
            ImGui::TextDisabled("Status: %s | Speed: %.1fx", 
                isPaused ? "PAUSED" : "RUNNING", 
                sceneManager.getSimulationSpeed());
        }
    }
    ImGui::End();
}
