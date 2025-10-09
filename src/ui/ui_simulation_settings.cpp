#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../scene/scene_manager.h"
#include "../../third_party/imgui/imgui.h"

void UIManager::renderSimulationSettings(CellManager& cellManager)
{
    ImGui::SetNextWindowPos(ImVec2(10, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Simulation Settings", nullptr, getWindowFlags()))
    {
        if (ImGui::CollapsingHeader("Voxel Grid (Nutrients)", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Separator();
            
            // Visualization toggles
            ImGui::Checkbox("Show Grid Lines", &showVoxelGrid);
            ImGui::SameLine();
            addTooltip("Toggle 16³ voxel grid lines (much coarser than 64³ spatial grid)");
            
            ImGui::Checkbox("Show Nutrient Voxels", &showVoxelCubes);
            ImGui::SameLine();
            addTooltip("Toggle colored cubes for voxels containing nutrients\nColors represent nutrient density and composition");
            
            ImGui::Separator();
            
            // Get voxel manager config reference
            auto& voxelConfig = cellManager.getVoxelManager().getConfig();
            
            // Display statistics (count is GPU-managed for performance)
            ImGui::Text("Active Nutrient Voxels: GPU-managed (indirect rendering)");
            ImGui::Text("Grid Resolution: %d³ (%d total)", 
                        voxelConfig.resolution, 
                        voxelConfig.resolution * voxelConfig.resolution * voxelConfig.resolution);
            ImGui::Text("Voxel Size: %.2f units", voxelConfig.voxelSize);
            
            ImGui::Separator();
            
            // Cloud generation parameters
            ImGui::Text("Cloud Generation");
            
            drawSliderWithInput("Noise Scale", &voxelConfig.noiseScale, 0.01f, 0.2f, "%.3f");
            addTooltip("Scale of procedural noise (lower = larger irregular features)");
            
            drawSliderWithInput("Noise Strength", &voxelConfig.noiseStrength, 0.0f, 1.0f, "%.2f");
            addTooltip("How much noise distorts the cloud shape (0 = sphere, 1 = very irregular)");
            
            drawSliderWithInput("Density Falloff", &voxelConfig.densityFalloff, 0.5f, 3.0f, "%.2f");
            addTooltip("How quickly nutrients fade from center (lower = fuller clouds)");
            
            ImGui::Separator();
            
            ImGui::Text("Nutrient Distribution");
            drawSliderWithInput("Nutrient Gradient", &voxelConfig.nutrientDensityGradient, 0.1f, 10.0f, "%.2f");
            addTooltip("Peak nutrient density at cloud center\n"
                      "0.1 = Very sparse nutrients\n"
                      "1.0 = Default density\n"
                      "10.0 = Very dense nutrients");
            
            drawSliderWithInput("Nutrient Falloff", &voxelConfig.nutrientDensityFalloff, 0.5f, 10.0f, "%.2f");
            addTooltip("How quickly nutrient density decreases from center\n"
                      "0.5 = Gradual falloff (nutrients spread wide)\n"
                      "2.0 = Default falloff\n"
                      "10.0 = Sharp falloff (nutrients concentrated at center)");
            
            // Calculate and display average nutrients per cloud
            float avgRadius = (voxelConfig.minCloudRadius + voxelConfig.maxCloudRadius) * 0.5f;
            float cloudVolume = (4.0f / 3.0f) * 3.14159f * avgRadius * avgRadius * avgRadius;
            // Approximate integral: density * volume / (1 + falloff)
            float avgNutrients = voxelConfig.nutrientDensityGradient * cloudVolume / (1.0f + voxelConfig.nutrientDensityFalloff);
            ImGui::Text("Avg Nutrients/Cloud: %.0f units", avgNutrients);
            addTooltip("Estimated average total nutrients per cloud\n"
                      "Based on current gradient, falloff, and cloud size");
            
            ImGui::Separator();
            
            ImGui::Text("Cloud Size");
            drawSliderWithInput("Min Radius", &voxelConfig.minCloudRadius, 5.0f, 25.0f, "%.1f");
            addTooltip("Minimum cloud radius");
            
            drawSliderWithInput("Max Radius", &voxelConfig.maxCloudRadius, 15.0f, 50.0f, "%.1f");
            addTooltip("Maximum cloud radius");
            
            // Ensure min <= max
            if (voxelConfig.minCloudRadius > voxelConfig.maxCloudRadius)
            {
                voxelConfig.maxCloudRadius = voxelConfig.minCloudRadius;
            }
            
            ImGui::Separator();
            
            ImGui::Text("Cloud Spawning");
            drawSliderWithInput("Spawn Interval", &voxelConfig.cloudSpawnInterval, 1.0f, 10.0f, "%.1f");
            addTooltip("Base time between cloud spawns (seconds)");
            
            drawSliderWithInput("Spawn Variance", &voxelConfig.cloudSpawnVariance, 0.0f, 5.0f, "%.1f");
            addTooltip("Random variation in spawn timing (seconds)");
            
            ImGui::Separator();
            
            ImGui::Text("Nutrient Decay");
            drawSliderWithInput("Decay Rate", &voxelConfig.decayRate, 0.0f, 0.5f, "%.3f");
            addTooltip("How quickly nutrients disappear over time");
            
            ImGui::Separator();
            
            ImGui::Text("Visualization");
            
            ImGui::Checkbox("Show Nutrient Particles", &cellManager.getVoxelManager().showNutrientParticles);
            ImGui::SameLine();
            addTooltip("Show billboard particles for nutrient voxels\n"
                      "One particle per 16³ voxel grid cell with nutrients");
            
            if (cellManager.getVoxelManager().showNutrientParticles)
            {
                drawSliderWithInput("Particle Size", &cellManager.getVoxelManager().particleSize, 0.1f, 2.0f, "%.2f");
                addTooltip("Size of nutrient particles in world units");
                
                drawSliderWithInput("Particle Jitter", &cellManager.getVoxelManager().particleJitter, 0.0f, 2.0f, "%.2f");
                addTooltip("Random position offset for particles\n"
                          "0 = grid-aligned (uniform)\n"
                          "1 = up to half cell size offset\n"
                          "2 = up to full cell size offset (maximum)");
            }
            
            drawSliderWithInput("Color Sensitivity", &cellManager.getVoxelManager().colorSensitivity, 0.1f, 5.0f, "%.2f");
            addTooltip("Controls how sensitive colors are to nutrient density\n"
                      "Lower = darker colors need more nutrients\n"
                      "Higher = brighter colors even with low nutrients");
            
            ImGui::Separator();
            
            // Manual cloud spawn button
            if (ImGui::Button("Spawn Test Cloud", ImVec2(-1, 0)))
            {
                glm::vec3 center(0.0f, 0.0f, 0.0f);
                glm::vec3 color(0.8f, 0.3f, 0.9f);
                cellManager.getVoxelManager().spawnCloud(center, 12.0f, color);
            }
            addTooltip("Manually spawn a nutrient cloud at world origin for testing");
        }
        
        if (ImGui::CollapsingHeader("Physics Settings"))
        {
            ImGui::Separator();
            ImGui::Text("Global Physics");
            
            drawSliderWithInput("Global Drag", &cellManager.globalDrag, 0.0f, 1.0f, "%.3f");
            addTooltip("Global drag coefficient applied to all cells\n"
                      "0.0 = No drag (perpetual motion)\n"
                      "0.02 = Low drag (default, smooth motion)\n"
                      "1.0 = High drag (cells stop quickly)");
            
            ImGui::Separator();
            ImGui::Text("Gravity (units/s²)");
            
            drawSliderWithInput("Gravity X", &cellManager.globalGravity.x, -10.0f, 10.0f, "%.2f");
            drawSliderWithInput("Gravity Y", &cellManager.globalGravity.y, -10.0f, 10.0f, "%.2f");
            drawSliderWithInput("Gravity Z", &cellManager.globalGravity.z, -10.0f, 10.0f, "%.2f");
            addTooltip("Global gravity acceleration vector\n"
                      "Default: (0, 0, 0) - no gravity\n"
                      "Example: (0, -9.8, 0) - Earth-like downward gravity");
            
            if (ImGui::Button("Reset Gravity", ImVec2(-1, 0)))
            {
                cellManager.globalGravity = glm::vec3(0.0f, 0.0f, 0.0f);
            }
            addTooltip("Reset gravity to zero");
            
            ImGui::Separator();
        }
    }
    
    ImGui::End();
}
