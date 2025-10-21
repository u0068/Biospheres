#include "ui_manager.h"
#include "../input/injection_system.h"
#include "../simulation/spatial/spatial_grid_system.h"
#include "../rendering/systems/visualization_renderer.h"
#include "imgui_helpers.h"
#include <GLFW/glfw3.h>
#include <iostream>

void UIManager::renderInjectionControls(InjectionSystem& injectionSystem, SpatialGridSystem& spatialGrid, VisualizationRenderer& visualizationRenderer) {
    // Requirement 7.5: Add visual feedback for current distance (cell or injection plane)
    
    int flags = getWindowFlags(ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::Begin("Injection Controls", nullptr, flags)) {
        
        // Display current mode and distance information
        std::string distanceInfo = injectionSystem.getCurrentDistanceInfo();
        ImGui::Text("%s", distanceInfo.c_str());
        
        ImGui::Separator();
        
        // Mode switching buttons
        ImGui::Text("Mode Selection:");
        if (ImGui::Button("1 - Cell Selection")) {
            injectionSystem.handleKeyInput(GLFW_KEY_1);
        }
        ImGui::SameLine();
        if (ImGui::Button("2 - Density Injection")) {
            injectionSystem.handleKeyInput(GLFW_KEY_2);
        }
        ImGui::SameLine();
        if (ImGui::Button("3 - Velocity Injection")) {
            injectionSystem.handleKeyInput(GLFW_KEY_3);
        }
        
        ImGui::Separator();
        
        // Injection parameters (only show in injection modes)
        InjectionMode currentMode = injectionSystem.getCurrentMode();
        if (currentMode == InjectionMode::Density || currentMode == InjectionMode::Velocity) {
            
            ImGui::Text("Injection Parameters:");
            
            // Injection radius
            float radius = injectionSystem.getInjectionRadius();
            if (ImGui::SliderFloat("Radius", &radius, 0.1f, 10.0f, "%.1f")) {
                injectionSystem.setInjectionRadius(radius);
            }
            
            // Injection strength
            float strength = injectionSystem.getInjectionStrength();
            if (ImGui::SliderFloat("Strength", &strength, 0.0f, 5.0f, "%.2f")) {
                injectionSystem.setInjectionStrength(strength);
            }
            
            // Injection plane distance
            float planeDistance = injectionSystem.getInjectionPlaneDistance();
            if (ImGui::SliderFloat("Plane Distance", &planeDistance, -50.0f, 50.0f, "%.1f")) {
                injectionSystem.setInjectionPlaneDistance(planeDistance);
            }
            
            // Velocity direction (only for velocity mode)
            if (currentMode == InjectionMode::Velocity) {
                glm::vec3 velocityDir = injectionSystem.getVelocityDirection();
                float velocityArray[3] = {velocityDir.x, velocityDir.y, velocityDir.z};
                if (ImGui::SliderFloat3("Velocity Direction", velocityArray, -1.0f, 1.0f, "%.2f")) {
                    injectionSystem.setVelocityDirection(glm::vec3(velocityArray[0], velocityArray[1], velocityArray[2]));
                }
            }
            

            
            ImGui::Separator();
            
            // Clear fluid data button
            if (ImGui::Button("Clear All Fluid Data")) {
                spatialGrid.clearAllFluidData();
                std::cout << "InjectionControls: Cleared all fluid data" << std::endl;
            }
            
            ImGui::Separator();
            
            // Visualization controls
            ImGui::Text("Visualization:");
            
            // Get current config
            auto config = visualizationRenderer.getVisualizationConfig();
            bool configChanged = false;
            
            // Visualization mode checkboxes
            bool showWireframes = (config.visualizationMode & static_cast<int>(VisualizationRenderer::VisualizationMode::DensityWireframe)) != 0;
            bool showFlowLines = (config.visualizationMode & static_cast<int>(VisualizationRenderer::VisualizationMode::FlowLines)) != 0;

            
            if (ImGui::Checkbox("Density Wireframes", &showWireframes)) {
                if (showWireframes) {
                    config.visualizationMode |= static_cast<int>(VisualizationRenderer::VisualizationMode::DensityWireframe);
                } else {
                    config.visualizationMode &= ~static_cast<int>(VisualizationRenderer::VisualizationMode::DensityWireframe);
                }
                configChanged = true;
            }
            
            if (ImGui::Checkbox("Flow Lines", &showFlowLines)) {
                if (showFlowLines) {
                    config.visualizationMode |= static_cast<int>(VisualizationRenderer::VisualizationMode::FlowLines);
                } else {
                    config.visualizationMode &= ~static_cast<int>(VisualizationRenderer::VisualizationMode::FlowLines);
                }
                configChanged = true;
            }
            

            
            // Visualization parameters
            if (showWireframes || showFlowLines) {
                ImGui::Separator();
                ImGui::Text("Visualization Settings:");
                
                if (ImGui::SliderFloat("Density Threshold", &config.densityThreshold, 0.001f, 1.0f, "%.3f")) {
                    configChanged = true;
                }
                
                if (ImGui::SliderFloat("Max Density", &config.maxDensity, 0.1f, 10.0f, "%.1f")) {
                    configChanged = true;
                }
                
                if (showFlowLines) {
                    if (ImGui::SliderFloat("Min Velocity Threshold", &config.minVelocityThreshold, 0.001f, 1.0f, "%.3f")) {
                        configChanged = true;
                    }
                    
                    if (ImGui::SliderFloat("Max Velocity", &config.maxVelocity, 1.0f, 50.0f, "%.1f")) {
                        configChanged = true;
                    }
                    
                    if (ImGui::SliderFloat("Max Line Length", &config.maxLineLength, 0.5f, 10.0f, "%.1f")) {
                        configChanged = true;
                    }
                }
            }
            
            // Apply config changes
            if (configChanged) {
                visualizationRenderer.setVisualizationConfig(config);
                std::cout << "Visualization config updated - Mode: " << config.visualizationMode 
                         << ", Density threshold: " << config.densityThreshold << std::endl;
            }
            
            // Performance statistics
            if (showWireframes || showFlowLines) {
                ImGui::Separator();
                ImGui::Text("Performance Stats:");
                
                size_t totalVoxels = visualizationRenderer.getTotalVoxelCount();
                size_t compactVoxels = visualizationRenderer.getCompactVoxelCount();
                float skipRatio = visualizationRenderer.getVoxelSkipRatio() * 100.0f;
                
                ImGui::Text("Total voxels: %zu", totalVoxels);
                ImGui::Text("Active voxels: %zu", compactVoxels);
                ImGui::Text("Skipped: %.1f%%", skipRatio);
                
                if (skipRatio > 90.0f) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "(Excellent!)");
                } else if (skipRatio > 70.0f) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "(Good)");
                } else if (skipRatio < 50.0f) {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "(Poor)");
                }
            }
        }
        
        // Instructions
        ImGui::Separator();
        ImGui::Text("Instructions:");
        ImGui::Text("- Keys 1/2/3: Switch modes");
        ImGui::Text("- Mouse: Move brush / Click to inject");
        ImGui::Text("- Scroll: Adjust distance (mode-specific)");
        
        // Brush visibility status
        if (injectionSystem.isBrushVisible()) {
            glm::vec3 brushPos = injectionSystem.getBrushPosition();
            ImGui::Text("Brush Position: (%.1f, %.1f, %.1f)", brushPos.x, brushPos.y, brushPos.z);
        } else {
            ImGui::Text("Brush: Hidden");
        }
    }
    ImGui::End();
}