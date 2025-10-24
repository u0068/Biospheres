#include "baker_ui.h"
#include "mesh_baker.h"

// Third-party includes
#include <GLFW/glfw3.h>

// Standard includes
#include <iostream>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <cstring>

// Helper function for Catmull-Rom spline interpolation
namespace {
    float catmullRomInterpolate(float t, float p0, float p1, float p2, float p3) {
        float t2 = t * t;
        float t3 = t2 * t;
        return 0.5f * ((2.0f * p1) +
                      (-p0 + p2) * t +
                      (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                      (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    }
}

BakerUI::BakerUI()
{
    // Initialize output directory buffer
    strncpy_s(outputDirBuffer, sizeof(outputDirBuffer), config.outputDirectory.c_str(), _TRUNCATE);
    
    // Try to load last used configuration
    std::string configPath = getDefaultConfigPath();
    if (std::filesystem::exists(configPath))
    {
        std::cout << "Loading last used configuration from: " << configPath << "\n";
        loadConfiguration(configPath);
    }
    else
    {
        std::cout << "No previous configuration found, using defaults\n";
    }
}

void BakerUI::render(MeshBaker& meshBaker)
{
    // Render main menu bar
    renderMainMenuBar();
    
    // Create dockspace
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    
    ImGui::Begin("DockSpace", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    
    // Create dockspace
    ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    ImGui::End();
    
    // Render UI panels
    renderConfigPanel();
    renderParameterControls();
    renderBakingControls(meshBaker);
    renderPreviewControls();
    renderPreviewViewport();
    
    // Render progress dialog if active
    if (showProgressDialog)
    {
        renderProgressDialog();
    }
    
    // Render completion dialog if active
    if (showCompletionDialog)
    {
        renderCompletionDialog();
    }
    
    // Render error dialog if active
    if (showErrorDialog)
    {
        renderErrorDialog();
    }
}

void BakerUI::renderMainMenuBar()
{
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Save Configuration"))
            {
                saveConfiguration(getDefaultConfigPath());
            }
            
            if (ImGui::MenuItem("Load Configuration"))
            {
                loadConfiguration(getDefaultConfigPath());
            }
            
            ImGui::Separator();
            
            if (ImGui::MenuItem("Exit"))
            {
                // Request application exit
                glfwSetWindowShouldClose(glfwGetCurrentContext(), GLFW_TRUE);
            }
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View"))
        {
            // TODO: Add view options
            ImGui::MenuItem("Show Preview", nullptr, true);
            ImGui::MenuItem("Show Configuration", nullptr, true);
            
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Help"))
        {
            if (ImGui::MenuItem("About"))
            {
                // TODO: Show about dialog
                std::cout << "About dialog requested\n";
            }
            
            ImGui::EndMenu();
        }
        
        ImGui::EndMainMenuBar();
    }
}

void BakerUI::renderConfigPanel()
{
    ImGui::Begin("Baking Configuration");
    
    // Output settings
    renderOutputSettings();
    
    ImGui::Separator();
    
    // Quality settings
    renderQualitySettings();
    
    ImGui::Separator();
    
    // Size ratio configuration
    renderSizeRatioConfig();
    
    ImGui::Separator();
    
    // Distance ratio configuration
    renderDistanceRatioConfig();
    
    ImGui::Separator();
    
    // Summary information
    ImGui::Text("Total Meshes: %d", config.getTotalMeshCount());
    
    // Validation status
    if (!validateConfiguration())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("Configuration Errors:");
        ImGui::Text("%s", getValidationErrors().c_str());
        ImGui::PopStyleColor();
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
        ImGui::Text("Configuration Valid");
        ImGui::PopStyleColor();
    }
    
    ImGui::End();
}

void BakerUI::renderParameterControls()
{
    ImGui::Begin("Preview Parameters");
    
    // Size ratio slider
    ImGui::Text("Size Ratio (Larger / Smaller)");
    if (ImGui::SliderFloat("##SizeRatio", &currentSizeRatio, 1.0f, 4.0f, "%.2f"))
    {
        // Real-time preview update
    }
    
    // Distance ratio slider
    ImGui::Text("Distance Ratio (Distance / Combined Radius)");
    if (ImGui::SliderFloat("##DistanceRatio", &currentDistanceRatio, 0.1f, 3.0f, "%.2f"))
    {
        // Real-time preview update
    }
    
    ImGui::Separator();
    
    // Preset buttons
    ImGui::Text("Presets:");
    
    if (ImGui::Button("Touching (1:1, 1.0)"))
    {
        currentSizeRatio = 1.0f;
        currentDistanceRatio = 1.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Separated (1:1, 2.0)"))
    {
        currentSizeRatio = 1.0f;
        currentDistanceRatio = 2.0f;
    }
    
    if (ImGui::Button("Small+Large (1:3, 1.0)"))
    {
        currentSizeRatio = 3.0f;
        currentDistanceRatio = 1.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Distant (2:1, 2.5)"))
    {
        currentSizeRatio = 2.0f;
        currentDistanceRatio = 2.5f;
    }
    
    ImGui::Separator();
    
    // Blend Strength Presets
    ImGui::Text("Blend Presets:");
    
    if (ImGui::Button("Sharp"))
    {
        config.blendingStrength = 0.2f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Normal"))
    {
        config.blendingStrength = 0.5f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Smooth"))
    {
        config.blendingStrength = 1.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Very Smooth"))
    {
        config.blendingStrength = 1.5f;
    }
    
    ImGui::Separator();
    
    // SDF Blending Controls
    ImGui::Text("SDF Blending Controls");
    
    // Calculate effective blending strength for display
    float effectiveBlendingStrength = config.blendingStrength;
    if (!config.blendCurvePoints.empty())
    {
        float smallRadius = 1.0f;
        float largeRadius = smallRadius * currentSizeRatio;
        float combinedRadius = smallRadius + largeRadius;
        float actualDistance = currentDistanceRatio * combinedRadius;
        // Interpolate blend multiplier from control points using Catmull-Rom spline
        float blendMultiplier = 1.0f;
        if (!config.blendCurvePoints.empty())
        {
            if (currentDistanceRatio <= config.blendCurvePoints.front().distanceRatio)
            {
                blendMultiplier = config.blendCurvePoints.front().blendMultiplier;
            }
            else if (currentDistanceRatio >= config.blendCurvePoints.back().distanceRatio)
            {
                blendMultiplier = config.blendCurvePoints.back().blendMultiplier;
            }
            else
            {
                // Catmull-Rom spline interpolation
                for (size_t i = 0; i < config.blendCurvePoints.size() - 1; ++i)
                {
                    if (currentDistanceRatio >= config.blendCurvePoints[i].distanceRatio &&
                        currentDistanceRatio <= config.blendCurvePoints[i + 1].distanceRatio)
                    {
                        float t = (currentDistanceRatio - config.blendCurvePoints[i].distanceRatio) /
                                 (config.blendCurvePoints[i + 1].distanceRatio - config.blendCurvePoints[i].distanceRatio);
                        
                        // Get 4 control points for Catmull-Rom
                        float p0 = (i > 0) ? config.blendCurvePoints[i - 1].blendMultiplier : config.blendCurvePoints[i].blendMultiplier;
                        float p1 = config.blendCurvePoints[i].blendMultiplier;
                        float p2 = config.blendCurvePoints[i + 1].blendMultiplier;
                        float p3 = (i + 2 < config.blendCurvePoints.size()) ? config.blendCurvePoints[i + 2].blendMultiplier : config.blendCurvePoints[i + 1].blendMultiplier;
                        
                        blendMultiplier = catmullRomInterpolate(t, p0, p1, p2, p3);
                        break;
                    }
                }
            }
        }
        effectiveBlendingStrength = config.blendingStrength * blendMultiplier;
    }
    
    // Show the blend strength slider - displays effective value but controls base value
    float displayValue = effectiveBlendingStrength;
    if (ImGui::SliderFloat("Blend Strength", &displayValue, 0.1f, 10.0f, "%.2f"))
    {
        // When user drags, update the base strength to maintain the difference
        config.blendingStrength = displayValue - (effectiveBlendingStrength - config.blendingStrength);
        config.blendingStrength = std::max(0.1f, std::min(5.0f, config.blendingStrength));
    }
    if (ImGui::IsItemHovered())
    {
        if (effectiveBlendingStrength != config.blendingStrength)
        {
            float curveFactor = effectiveBlendingStrength / config.blendingStrength;
            ImGui::SetTooltip("Effective blend strength (%.2f base × %.2f curve)\nDrag to adjust base strength", 
                             config.blendingStrength, curveFactor);
        }
        else
        {
            ImGui::SetTooltip("Controls how smoothly the spheres blend together.\nLower = sharper transition, Higher = smoother/wider blend");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset##BlendingStrength"))
    {
        config.blendingStrength = 0.5f;
    }
    
    // Blend curve control - simplified since dragging is primary interaction
    ImGui::Text("Blend Strength Curve:");
    ImGui::Text("Control Points: %d", static_cast<int>(config.blendCurvePoints.size()));
    
    if (ImGui::Button("Reset to Default Curve"))
    {
        config.blendCurvePoints = {
            {0.1f, 0.5f},
            {1.0f, 1.0f},
            {2.0f, 1.5f},
            {3.0f, 0.2f}
        };
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Points"))
    {
        config.blendCurvePoints = {{1.5f, 1.0f}, {2.0f, 1.0f}};
    }
    
    // Show dynamic effective blending strength analysis
    if (!config.blendCurvePoints.empty())
    {
        ImGui::Separator();
        ImGui::Text("Distance Scaling Analysis");
        
        // Calculate the actual distance between spheres based on current parameters
        float smallRadius = 1.0f;
        float largeRadius = smallRadius * currentSizeRatio;
        float combinedRadius = smallRadius + largeRadius;
        float actualDistance = currentDistanceRatio * combinedRadius;
        
        // Calculate the effective blending strength using the same formula as the shader
        // Use same Catmull-Rom interpolation
        float blendMultiplier = 1.0f;
        if (!config.blendCurvePoints.empty())
        {
            if (currentDistanceRatio <= config.blendCurvePoints.front().distanceRatio)
            {
                blendMultiplier = config.blendCurvePoints.front().blendMultiplier;
            }
            else if (currentDistanceRatio >= config.blendCurvePoints.back().distanceRatio)
            {
                blendMultiplier = config.blendCurvePoints.back().blendMultiplier;
            }
            else
            {
                for (size_t i = 0; i < config.blendCurvePoints.size() - 1; ++i)
                {
                    if (currentDistanceRatio >= config.blendCurvePoints[i].distanceRatio &&
                        currentDistanceRatio <= config.blendCurvePoints[i + 1].distanceRatio)
                    {
                        float t = (currentDistanceRatio - config.blendCurvePoints[i].distanceRatio) /
                                 (config.blendCurvePoints[i + 1].distanceRatio - config.blendCurvePoints[i].distanceRatio);
                        
                        float p0 = (i > 0) ? config.blendCurvePoints[i - 1].blendMultiplier : config.blendCurvePoints[i].blendMultiplier;
                        float p1 = config.blendCurvePoints[i].blendMultiplier;
                        float p2 = config.blendCurvePoints[i + 1].blendMultiplier;
                        float p3 = (i + 2 < config.blendCurvePoints.size()) ? config.blendCurvePoints[i + 2].blendMultiplier : config.blendCurvePoints[i + 1].blendMultiplier;
                        
                        blendMultiplier = catmullRomInterpolate(t, p0, p1, p2, p3);
                        break;
                    }
                }
            }
        }
        float calculatedEffectiveBlendingStrength = config.blendingStrength * blendMultiplier;
        
        ImGui::Text("Effective Blend Strength: %.2f", calculatedEffectiveBlendingStrength);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "(Base: %.2f × Multiplier: %.2f)", 
                          config.blendingStrength, blendMultiplier);
        
        // Visual progress bar showing the scaling effect
        float maxBlendForBar = 10.0f; // Maximum expected blend strength for the bar
        float barProgress = std::min(calculatedEffectiveBlendingStrength / maxBlendForBar, 1.0f);
        
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.8f, 0.2f, 0.8f)); // Green
        ImGui::ProgressBar(barProgress, ImVec2(-1, 0), "");
        ImGui::PopStyleColor();
        
        // Show the breakdown
        ImGui::Text("Distance Ratio: %.2f -> Blend Multiplier: %.2f", currentDistanceRatio, blendMultiplier);
        ImGui::Text("Interpolated from %d control points", static_cast<int>(config.blendCurvePoints.size()));
        
        // Plot the curve
        ImGui::Spacing();
        ImGui::Text("Blend Curve Editor:");
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Drag red dots to adjust • Double-click to add • Right-click to delete");
        
        // Generate curve data
        constexpr int numPoints = 100;
        static float curveData[numPoints];
        float minDistance = 0.1f;
        float maxDistance = 3.0f;
        
        for (int i = 0; i < numPoints; ++i)
        {
            float distRatio = minDistance + (maxDistance - minDistance) * (i / float(numPoints - 1));
            
            // Catmull-Rom spline interpolation
            float blendMult = 1.0f;
            if (!config.blendCurvePoints.empty())
            {
                if (distRatio <= config.blendCurvePoints.front().distanceRatio)
                {
                    blendMult = config.blendCurvePoints.front().blendMultiplier;
                }
                else if (distRatio >= config.blendCurvePoints.back().distanceRatio)
                {
                    blendMult = config.blendCurvePoints.back().blendMultiplier;
                }
                else
                {
                    for (size_t j = 0; j < config.blendCurvePoints.size() - 1; ++j)
                    {
                        if (distRatio >= config.blendCurvePoints[j].distanceRatio &&
                            distRatio <= config.blendCurvePoints[j + 1].distanceRatio)
                        {
                            float t = (distRatio - config.blendCurvePoints[j].distanceRatio) /
                                     (config.blendCurvePoints[j + 1].distanceRatio - config.blendCurvePoints[j].distanceRatio);
                            
                            float p0 = (j > 0) ? config.blendCurvePoints[j - 1].blendMultiplier : config.blendCurvePoints[j].blendMultiplier;
                            float p1 = config.blendCurvePoints[j].blendMultiplier;
                            float p2 = config.blendCurvePoints[j + 1].blendMultiplier;
                            float p3 = (j + 2 < config.blendCurvePoints.size()) ? config.blendCurvePoints[j + 2].blendMultiplier : config.blendCurvePoints[j + 1].blendMultiplier;
                            
                            blendMult = catmullRomInterpolate(t, p0, p1, p2, p3);
                            break;
                        }
                    }
                }
            }
            curveData[i] = blendMult;
        }
        
        // Plot the curve with current position indicator
        float maxAmplitude = 1.0f;
        for (const auto& point : config.blendCurvePoints)
        {
            maxAmplitude = std::max(maxAmplitude, point.blendMultiplier);
        }
        maxAmplitude *= 1.1f; // Add 10% margin
        
        // Get plot position and size
        ImVec2 plotPos = ImGui::GetCursorScreenPos();
        ImVec2 plotSize(-1, 200);
        if (plotSize.x < 0) plotSize.x = ImGui::GetContentRegionAvail().x;
        
        ImGui::PlotLines("##BellCurve", curveData, numPoints, 0, nullptr, 0.0f, maxAmplitude, plotSize);
        
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        
        // Handle dragging control points
        static int draggedPointIndex = -1;
        static bool isDragging = false;
        
        ImVec2 mousePos = ImGui::GetMousePos();
        bool mouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        bool mouseReleased = ImGui::IsMouseReleased(ImGuiMouseButton_Left);
        
        // Check if mouse is over the plot area
        bool mouseOverPlot = mousePos.x >= plotPos.x && mousePos.x <= plotPos.x + plotSize.x &&
                            mousePos.y >= plotPos.y && mousePos.y <= plotPos.y + plotSize.y;
        
        // Draw and handle control points
        for (size_t i = 0; i < config.blendCurvePoints.size(); ++i)
        {
            float pointX = (config.blendCurvePoints[i].distanceRatio - minDistance) / (maxDistance - minDistance);
            float pointY = 1.0f - (config.blendCurvePoints[i].blendMultiplier / maxAmplitude);
            
            if (pointX >= 0.0f && pointX <= 1.0f && pointY >= 0.0f && pointY <= 1.0f)
            {
                ImVec2 center(plotPos.x + pointX * plotSize.x, plotPos.y + pointY * plotSize.y);
                float radius = 6.0f;
                
                // Check if mouse is over this point
                float distToMouse = std::sqrt((mousePos.x - center.x) * (mousePos.x - center.x) +
                                             (mousePos.y - center.y) * (mousePos.y - center.y));
                bool mouseOverPoint = distToMouse < radius + 3.0f;
                
                // Start dragging
                if (mouseOverPoint && mouseClicked && draggedPointIndex == -1)
                {
                    draggedPointIndex = static_cast<int>(i);
                    isDragging = true;
                }
                
                // Update point position while dragging
                if (isDragging && draggedPointIndex == static_cast<int>(i) && mouseDown && mouseOverPlot)
                {
                    // Convert mouse position to distance ratio and multiplier
                    float newX = (mousePos.x - plotPos.x) / plotSize.x;
                    float newY = 1.0f - (mousePos.y - plotPos.y) / plotSize.y;
                    
                    config.blendCurvePoints[i].distanceRatio = minDistance + newX * (maxDistance - minDistance);
                    config.blendCurvePoints[i].blendMultiplier = newY * maxAmplitude;
                    
                    // Clamp values
                    config.blendCurvePoints[i].distanceRatio = std::max(minDistance, std::min(maxDistance, config.blendCurvePoints[i].distanceRatio));
                    config.blendCurvePoints[i].blendMultiplier = std::max(0.0f, std::min(10.0f, config.blendCurvePoints[i].blendMultiplier));
                }
                
                // Draw point
                ImU32 pointColor = (mouseOverPoint || draggedPointIndex == static_cast<int>(i)) ? 
                                  IM_COL32(255, 150, 150, 255) : IM_COL32(255, 100, 100, 255);
                float drawRadius = (mouseOverPoint || draggedPointIndex == static_cast<int>(i)) ? radius + 1.0f : radius;
                
                drawList->AddCircleFilled(center, drawRadius, pointColor);
                drawList->AddCircle(center, drawRadius, IM_COL32(255, 255, 255, 255), 0, 2.0f);
                
                // Show tooltip with values
                if (mouseOverPoint && !isDragging)
                {
                    ImGui::SetTooltip("Distance: %.2f\nMultiplier: %.2f\nDrag to move\nRight-click to delete", 
                                     config.blendCurvePoints[i].distanceRatio,
                                     config.blendCurvePoints[i].blendMultiplier);
                }
                
                // Right-click to delete (but keep at least 2 points)
                if (mouseOverPoint && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && config.blendCurvePoints.size() > 2)
                {
                    config.blendCurvePoints.erase(config.blendCurvePoints.begin() + i);
                    break;
                }
            }
        }
        
        // Stop dragging
        if (mouseReleased)
        {
            if (isDragging)
            {
                // Sort points by distance after dragging
                std::sort(config.blendCurvePoints.begin(), config.blendCurvePoints.end(),
                         [](const auto& a, const auto& b) { return a.distanceRatio < b.distanceRatio; });
            }
            draggedPointIndex = -1;
            isDragging = false;
        }
        
        // Double-click to add new point
        if (mouseOverPlot && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !isDragging)
        {
            float newX = (mousePos.x - plotPos.x) / plotSize.x;
            float newY = 1.0f - (mousePos.y - plotPos.y) / plotSize.y;
            
            float newDist = minDistance + newX * (maxDistance - minDistance);
            float newMult = newY * maxAmplitude;
            
            config.blendCurvePoints.push_back({newDist, newMult});
            std::sort(config.blendCurvePoints.begin(), config.blendCurvePoints.end(),
                     [](const auto& a, const auto& b) { return a.distanceRatio < b.distanceRatio; });
        }
        
        // Draw vertical line at current distance ratio
        float currentPos = (currentDistanceRatio - minDistance) / (maxDistance - minDistance);
        if (currentPos >= 0.0f && currentPos <= 1.0f)
        {
            float lineX = plotPos.x + currentPos * plotSize.x;
            ImU32 lineColor = IM_COL32(255, 255, 0, 200); // Yellow
            drawList->AddLine(ImVec2(lineX, plotPos.y), ImVec2(lineX, plotPos.y + plotSize.y), lineColor, 2.0f);
            
            // Add arrow at top
            drawList->AddTriangleFilled(
                ImVec2(lineX, plotPos.y - 5),
                ImVec2(lineX - 5, plotPos.y),
                ImVec2(lineX + 5, plotPos.y),
                lineColor
            );
        }
        
        // Add labels
        ImGui::Text("Distance Ratio: 0.1");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 50);
        ImGui::Text("3.0");
        
        // Highlight curve effect
        if (blendMultiplier < 0.95f)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "→ Blend strength reduced by curve - thinner bridge");
        }
        else if (blendMultiplier > 1.05f)
        {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "→ Blend strength boosted by curve - thicker bridge");
        }
    }

    
    ImGui::Separator();
    
    // Reset buttons
    if (ImGui::Button("Reset Size Ratio"))
    {
        currentSizeRatio = 1.0f;
    }
    ImGui::SameLine();
    
    if (ImGui::Button("Reset Distance Ratio"))
    {
        currentDistanceRatio = 1.0f;
    }
    
    ImGui::End();
}

void BakerUI::renderBakingControls(MeshBaker& meshBaker)
{
    ImGui::Begin("Baking Operations");
    
    // Baking button
    bool canBake = validateConfiguration() && !bakingInProgress;
    
    // Test single mesh button
    if (ImGui::Button("Generate Test Mesh", ImVec2(-1, 30)))
    {
        std::cout << "\n=== Testing Single Mesh Generation ===\n";
        std::cout << "Size Ratio: " << currentSizeRatio << "\n";
        std::cout << "Distance Ratio: " << currentDistanceRatio << "\n";
        std::cout << "Resolution: " << config.resolution << "\n";
        
        testMesh = meshBaker.bakeDumbbellMesh(
            currentSizeRatio, 
            currentDistanceRatio, 
            config.resolution,
            config.blendingStrength,
            config.blendCurvePoints
        );
        
        if (testMesh.isValid())
        {
            std::cout << "SUCCESS: Generated valid mesh!\n";
            std::cout << "  Vertices: " << testMesh.positions.size() << "\n";
            std::cout << "  Triangles: " << (testMesh.indices.size() / 3) << "\n";
            std::cout << "  Normals: " << testMesh.normals.size() << "\n";
            showMeshPreview = true;
            meshChanged = true; // Signal that mesh needs re-upload
            std::cout << "  Mesh preview enabled: " << showMeshPreview << "\n";
        }
        else
        {
            std::cout << "ERROR: Generated invalid mesh!\n";
            showMeshPreview = false;
        }
        std::cout << "=================================\n\n";
    }
    
    ImGui::Spacing();
    
    if (!canBake)
    {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Button("Bake Parameter Grid", ImVec2(-1, 40)))
    {
        startBaking(meshBaker);
    }
    
    if (!canBake)
    {
        ImGui::EndDisabled();
    }
    
    // Status information
    if (bakingInProgress)
    {
        ImGui::Text("Baking in progress...");
        ImGui::ProgressBar(bakingProgress, ImVec2(-1, 0), bakingStatus.c_str());
    }
    else
    {
        ImGui::Text("Ready to bake %d mesh variations", config.getTotalMeshCount());
    }
    
    ImGui::Separator();
    
    // Estimated baking time
    int totalMeshes = config.getTotalMeshCount();
    float estimatedTimePerMesh = 0.5f; // Rough estimate for 64^3 resolution
    if (config.resolution == 32) estimatedTimePerMesh = 0.1f;
    else if (config.resolution == 128) estimatedTimePerMesh = 2.0f;
    
    float totalEstimatedTime = totalMeshes * estimatedTimePerMesh;
    
    ImGui::Text("Estimated Time: %.1f seconds", totalEstimatedTime);
    ImGui::Text("Resolution: %d^3", config.resolution);
    ImGui::Text("Output Directory: %s", config.outputDirectory.c_str());
    
    ImGui::End();
}

void BakerUI::renderProgressDialog()
{
    ImGui::OpenPopup("Baking Progress");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 220));
    
    if (ImGui::BeginPopupModal("Baking Progress", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text("Baking mesh variations...");
        ImGui::Spacing();
        
        // Progress bar with percentage
        char progressText[64];
        snprintf(progressText, sizeof(progressText), "%.1f%%", bakingProgress * 100.0f);
        ImGui::ProgressBar(bakingProgress, ImVec2(-1, 0), progressText);
        
        ImGui::Spacing();
        
        // Current mesh being baked
        ImGui::Text("Current: %s", bakingStatus.c_str());
        ImGui::Text("Mesh %d of %d", currentMeshIndex, totalMeshes);
        
        ImGui::Spacing();
        
        // Estimated time remaining
        if (estimatedTimeRemaining > 0.0f)
        {
            int minutes = static_cast<int>(estimatedTimeRemaining / 60.0f);
            int seconds = static_cast<int>(estimatedTimeRemaining) % 60;
            
            if (minutes > 0)
            {
                ImGui::Text("Estimated time remaining: %d min %d sec", minutes, seconds);
            }
            else
            {
                ImGui::Text("Estimated time remaining: %d sec", seconds);
            }
        }
        else
        {
            ImGui::Text("Calculating time remaining...");
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Cancel button
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            cancelBaking();
        }
        
        // Show cancellation message if cancelled
        if (bakingCancelled)
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "Cancelling...");
        }
        
        ImGui::EndPopup();
    }
}

void BakerUI::renderPreviewControls()
{
    ImGui::Begin("Preview Controls");
    
    // Preview mode toggle
    ImGui::Text("Preview Mode:");
    if (ImGui::RadioButton("Raymarching (SDF)", !showMeshPreview))
    {
        std::cout << "UI: Switched to Raymarching mode\n";
        showMeshPreview = false;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("Mesh Preview", showMeshPreview))
    {
        if (testMesh.isValid())
        {
            std::cout << "UI: Switched to Mesh Preview mode\n";
            std::cout << "  testMesh vertices: " << testMesh.positions.size() << "\n";
            showMeshPreview = true;
        }
        else
        {
            std::cout << "UI: Cannot switch to Mesh Preview - no valid mesh\n";
            showMeshPreview = false;
            ImGui::OpenPopup("No Mesh");
        }
    }
    
    // Debug: Show current state
    static bool lastShowMeshPreview = false;
    if (showMeshPreview != lastShowMeshPreview)
    {
        std::cout << "UI: showMeshPreview flag changed to: " << showMeshPreview << "\n";
        lastShowMeshPreview = showMeshPreview;
    }
    
    // Popup for no mesh available
    if (ImGui::BeginPopupModal("No Mesh", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("No mesh generated yet!");
        ImGui::Text("Click 'Generate Test Mesh' to create a mesh.");
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    
    if (showMeshPreview && testMesh.isValid())
    {
        ImGui::Text("Mesh Info:");
        ImGui::Text("  Vertices: %zu", testMesh.positions.size());
        ImGui::Text("  Triangles: %zu", testMesh.indices.size() / 3);
        
        ImGui::Spacing();
        ImGui::Checkbox("Wireframe Mode", &showWireframe);
    }
    
    ImGui::Separator();
    
    ImGui::Text("Camera Controls:");
    ImGui::BulletText("Right Mouse + Drag: Rotate camera");
    ImGui::BulletText("WASD: Move camera");
    ImGui::BulletText("Space/C: Move up/down");
    ImGui::BulletText("Q/E: Roll camera");
    
    ImGui::Separator();
    
    ImGui::Text("Current Parameters:");
    ImGui::Text("Size Ratio: %.2f", currentSizeRatio);
    ImGui::Text("Distance Ratio: %.2f", currentDistanceRatio);
    ImGui::Text("Blend Strength: %.2f", config.blendingStrength);
    ImGui::Text("Curve Points: %d", static_cast<int>(config.blendCurvePoints.size()));
    
    ImGui::End();
}

void BakerUI::renderPreviewViewport()
{
    ImGui::Begin("Preview Viewport");
    
    // Get the available content region
    ImVec2 contentRegion = ImGui::GetContentRegionAvail();
    
    // Make sure we have a reasonable minimum size
    if (contentRegion.x < 100.0f) contentRegion.x = 400.0f;
    if (contentRegion.y < 100.0f) contentRegion.y = 300.0f;
    
    // Store viewport size for the preview renderer
    viewportSize = glm::vec2(contentRegion.x, contentRegion.y);
    
    // Display the rendered texture (this will be set by the application)
    if (previewTexture != 0)
    {
        ImGui::Image(static_cast<ImTextureID>(static_cast<intptr_t>(previewTexture)), 
                    contentRegion, ImVec2(0, 1), ImVec2(1, 0)); // Flip Y coordinate
    }
    else
    {
        // Fallback display when no texture is available
        ImGui::BeginChild("3DViewport", contentRegion, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        
        // Add some text to indicate this is the 3D viewport
        ImGui::SetCursorPos(ImVec2(10, 10));
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "3D Preview");
        ImGui::SetCursorPos(ImVec2(10, 30));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Size: %.1f, Distance: %.1f", currentSizeRatio, currentDistanceRatio);
        ImGui::SetCursorPos(ImVec2(10, 50));
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Preview texture not available");
        
        ImGui::EndChild();
    }
    
    ImGui::End();
}

void BakerUI::renderSizeRatioConfig()
{
    ImGui::Text("Size Ratios (Larger / Smaller)");
    
    // List existing ratios
    for (size_t i = 0; i < config.sizeRatios.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        
        ImGui::Text("%.2f", config.sizeRatios[i]);
        ImGui::SameLine();
        
        if (ImGui::Button("Preview"))
        {
            // Set current size ratio to this value for preview
            currentSizeRatio = config.sizeRatios[i];
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Remove"))
        {
            removeSizeRatio(i);
        }
        
        ImGui::PopID();
    }
    
    // Add new ratio
    ImGui::InputFloat("New Size Ratio", &newSizeRatio, 0.1f, 1.0f, "%.2f");
    ImGui::SameLine();
    
    if (ImGui::Button("Add Size Ratio"))
    {
        addSizeRatio();
    }
}

void BakerUI::renderDistanceRatioConfig()
{
    ImGui::Text("Distance Ratios (Distance / Combined Radius)");
    
    // List existing ratios
    for (size_t i = 0; i < config.distanceRatios.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i + 1000)); // Offset to avoid ID conflicts
        
        ImGui::Text("%.2f", config.distanceRatios[i]);
        ImGui::SameLine();
        
        if (ImGui::Button("Preview"))
        {
            // Set current distance ratio to this value for preview
            // Keep current blend strength and distance scaling settings
            currentDistanceRatio = config.distanceRatios[i];
        }
        ImGui::SameLine();
        
        if (ImGui::Button("Remove"))
        {
            removeDistanceRatio(i);
        }
        
        ImGui::PopID();
    }
    
    // Add new ratio
    ImGui::InputFloat("New Distance Ratio", &newDistanceRatio, 0.1f, 1.0f, "%.2f");
    ImGui::SameLine();
    
    if (ImGui::Button("Add Distance Ratio"))
    {
        addDistanceRatio();
    }
}

void BakerUI::renderQualitySettings()
{
    ImGui::Text("Quality Settings");
    
    // Resolution dropdown
    const auto resolutionOptions = BakingConfig::getResolutionOptions();
    const char* resolutionLabels[] = {"32^3", "64^3", "128^3"};
    
    int currentResolutionIndex = 1; // Default to 64
    for (size_t i = 0; i < resolutionOptions.size(); ++i)
    {
        if (resolutionOptions[i] == config.resolution)
        {
            currentResolutionIndex = static_cast<int>(i);
            break;
        }
    }
    
    if (ImGui::Combo("Resolution", &currentResolutionIndex, resolutionLabels, 3))
    {
        config.resolution = resolutionOptions[currentResolutionIndex];
    }
    
    // Memory usage estimate
    int voxelCount = config.resolution * config.resolution * config.resolution;
    float memoryMB = (voxelCount * sizeof(float)) / (1024.0f * 1024.0f);
    ImGui::Text("Memory per mesh: %.1f MB", memoryMB);
}

void BakerUI::renderOutputSettings()
{
    ImGui::Text("Output Settings");
    
    // Output directory
    ImGui::InputText("Output Directory", outputDirBuffer, sizeof(outputDirBuffer));
    config.outputDirectory = std::string(outputDirBuffer);
    
    // Directory validation
    if (!std::filesystem::exists(config.outputDirectory))
    {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::Text("(Directory does not exist)");
        ImGui::PopStyleColor();
    }
}

void BakerUI::addSizeRatio()
{
    if (config.validateSizeRatio(newSizeRatio))
    {
        // Check for duplicates
        auto it = std::find_if(config.sizeRatios.begin(), config.sizeRatios.end(),
            [this](float ratio) { return std::abs(ratio - newSizeRatio) < 0.01f; });
        
        if (it == config.sizeRatios.end())
        {
            config.sizeRatios.push_back(newSizeRatio);
            std::sort(config.sizeRatios.begin(), config.sizeRatios.end());
        }
    }
}

void BakerUI::removeSizeRatio(size_t index)
{
    if (index < config.sizeRatios.size() && config.sizeRatios.size() > 1)
    {
        config.sizeRatios.erase(config.sizeRatios.begin() + index);
    }
}

void BakerUI::addDistanceRatio()
{
    if (config.validateDistanceRatio(newDistanceRatio))
    {
        // Check for duplicates
        auto it = std::find_if(config.distanceRatios.begin(), config.distanceRatios.end(),
            [this](float ratio) { return std::abs(ratio - newDistanceRatio) < 0.01f; });
        
        if (it == config.distanceRatios.end())
        {
            config.distanceRatios.push_back(newDistanceRatio);
            std::sort(config.distanceRatios.begin(), config.distanceRatios.end());
        }
    }
}

void BakerUI::removeDistanceRatio(size_t index)
{
    if (index < config.distanceRatios.size() && config.distanceRatios.size() > 1)
    {
        config.distanceRatios.erase(config.distanceRatios.begin() + index);
    }
}

void BakerUI::startBaking(MeshBaker& meshBaker)
{
    if (bakingInProgress)
    {
        return;
    }
    
    if (!validateConfiguration())
    {
        showError("Configuration Error", 
                  getValidationErrors(),
                  "Please fix the configuration errors before starting the baking process.");
        return;
    }
    
    bakingInProgress = true;
    bakingCancelled = false;
    showProgressDialog = true;
    bakingProgress = 0.0f;
    bakingStatus = "Initializing...";
    currentMeshIndex = 0;
    totalMeshes = config.getTotalMeshCount();
    estimatedTimeRemaining = 0.0f;
    lastError = "";
    
    std::cout << "Starting baking process with " << totalMeshes << " meshes\n";
    
    // Create output directory if it doesn't exist
    try
    {
        std::filesystem::create_directories(config.outputDirectory);
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to create output directory: " << e.what() << "\n";
        lastError = "Failed to create output directory";
        showError("File I/O Error",
                  std::string("Failed to create output directory: ") + e.what(),
                  "Check that you have write permissions for the specified directory.");
        finishBaking();
        return;
    }
    
    // NOTE: This implementation runs synchronously on the main thread.
    // The UI will remain responsive for cancellation, but won't update smoothly.
    // For better responsiveness, implement parallel baking with threading (optional optimization).
    
    // Start the actual baking process
    try
    {
        meshBaker.bakeAllVariations(config, 
            [this](float progress, const std::string& status) {
                updateBakingProgress(progress, status);
                // Return true to continue, false to cancel
                return !bakingCancelled;
            });
        
        // Baking completed successfully
        finishBaking();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Baking failed with exception: " << e.what() << "\n";
        lastError = std::string("Baking failed: ") + e.what();
        showError("Baking Error",
                  std::string("An error occurred during the baking process: ") + e.what(),
                  "Check the console output for more details. Verify that the output directory is writable and that there is sufficient disk space.");
        finishBaking();
    }
}

void BakerUI::updateBakingProgress(float progress, const std::string& status)
{
    bakingProgress = progress;
    bakingStatus = status;
    
    // Update current mesh index from progress
    currentMeshIndex = static_cast<int>(progress * totalMeshes);
    
    // Calculate estimated time remaining
    // This is a simple linear estimate based on progress
    if (progress > 0.01f)
    {
        // Estimate based on average time per mesh so far
        // Assuming roughly 0.5 seconds per mesh at 64^3 resolution
        float estimatedTotalTime = totalMeshes * 0.5f;
        if (config.resolution == 32) estimatedTotalTime = totalMeshes * 0.1f;
        else if (config.resolution == 128) estimatedTotalTime = totalMeshes * 2.0f;
        
        estimatedTimeRemaining = estimatedTotalTime * (1.0f - progress);
    }
}

void BakerUI::finishBaking()
{
    bakingInProgress = false;
    showProgressDialog = false;
    
    // Determine if baking succeeded
    bakingSucceeded = !bakingCancelled && lastError.empty();
    
    // Show completion dialog
    showCompletionDialog = true;
    
    if (bakingCancelled)
    {
        std::cout << "Baking process cancelled\n";
        lastError = "Baking was cancelled by user";
        bakingSucceeded = false;
    }
    else if (!lastError.empty())
    {
        std::cout << "Baking process failed: " << lastError << "\n";
    }
    else
    {
        std::cout << "Baking process completed successfully\n";
        std::cout << "Generated " << totalMeshes << " mesh variations\n";
        std::cout << "Output directory: " << config.outputDirectory << "\n";
    }
    
    bakingCancelled = false;
}

void BakerUI::cancelBaking()
{
    bakingCancelled = true;
    std::cout << "Baking cancellation requested\n";
}

bool BakerUI::validateConfiguration() const
{
    // Check output directory
    if (config.outputDirectory.empty())
    {
        return false;
    }
    
    // Check size ratios
    if (config.sizeRatios.empty())
    {
        return false;
    }
    
    for (float ratio : config.sizeRatios)
    {
        if (!config.validateSizeRatio(ratio))
        {
            return false;
        }
    }
    
    // Check distance ratios
    if (config.distanceRatios.empty())
    {
        return false;
    }
    
    for (float ratio : config.distanceRatios)
    {
        if (!config.validateDistanceRatio(ratio))
        {
            return false;
        }
    }
    
    return true;
}

std::string BakerUI::getValidationErrors() const
{
    std::string errors;
    
    if (config.outputDirectory.empty())
    {
        errors += "Output directory is empty\n";
    }
    
    if (config.sizeRatios.empty())
    {
        errors += "No size ratios configured\n";
    }
    
    if (config.distanceRatios.empty())
    {
        errors += "No distance ratios configured\n";
    }
    
    for (float ratio : config.sizeRatios)
    {
        if (!config.validateSizeRatio(ratio))
        {
            errors += "Invalid size ratio: " + std::to_string(ratio) + "\n";
        }
    }
    
    for (float ratio : config.distanceRatios)
    {
        if (!config.validateDistanceRatio(ratio))
        {
            errors += "Invalid distance ratio: " + std::to_string(ratio) + "\n";
        }
    }
    
    return errors;
}

void BakerUI::renderCompletionDialog()
{
    ImGui::OpenPopup("Baking Complete");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 180));
    
    if (ImGui::BeginPopupModal("Baking Complete", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        if (bakingSucceeded)
        {
            // Success message
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
            ImGui::Text("Baking completed successfully!");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Text("Total meshes generated: %d", totalMeshes);
            ImGui::Text("Output directory: %s", config.outputDirectory.c_str());
        }
        else
        {
            // Error message
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("Baking failed!");
            ImGui::PopStyleColor();
            
            ImGui::Spacing();
            ImGui::Text("Error: %s", lastError.c_str());
            ImGui::Text("Meshes completed: %d of %d", currentMeshIndex, totalMeshes);
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // OK button
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            showCompletionDialog = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void BakerUI::renderErrorDialog()
{
    ImGui::OpenPopup("Error");
    
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500, 250), ImGuiCond_Appearing);
    
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        // Error title
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
        ImGui::TextWrapped("%s", errorTitle.c_str());
        ImGui::PopStyleColor();
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // Error message
        ImGui::TextWrapped("Error Details:");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", errorMessage.c_str());
        
        // Recovery suggestion if provided
        if (!errorRecoverySuggestion.empty())
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            ImGui::TextWrapped("Suggestion:");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::TextWrapped("%s", errorRecoverySuggestion.c_str());
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        
        // OK button
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - 120) * 0.5f);
        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            showErrorDialog = false;
            errorTitle.clear();
            errorMessage.clear();
            errorRecoverySuggestion.clear();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

void BakerUI::showError(const std::string& title, const std::string& message, const std::string& recoverySuggestion)
{
    errorTitle = title;
    errorMessage = message;
    errorRecoverySuggestion = recoverySuggestion;
    showErrorDialog = true;
}

bool BakerUI::saveConfiguration(const std::string& filepath)
{
    try
    {
        std::ofstream file(filepath);
        if (!file.is_open())
        {
            showError("Configuration Save Error",
                      "Failed to open file for writing: " + filepath,
                      "Check that you have write permissions for the specified location.");
            return false;
        }
        
        // Write configuration in a simple text format
        file << "# Metaball Baker Configuration File\n";
        file << "# Generated by Metaball Baker Tool\n\n";
        
        // Output settings
        file << "[Output]\n";
        file << "directory=" << config.outputDirectory << "\n\n";
        
        // Quality settings
        file << "[Quality]\n";
        file << "resolution=" << config.resolution << "\n\n";
        
        // Size ratios
        file << "[SizeRatios]\n";
        file << "count=" << config.sizeRatios.size() << "\n";
        for (size_t i = 0; i < config.sizeRatios.size(); ++i)
        {
            file << "ratio" << i << "=" << config.sizeRatios[i] << "\n";
        }
        file << "\n";
        
        // Distance ratios
        file << "[DistanceRatios]\n";
        file << "count=" << config.distanceRatios.size() << "\n";
        for (size_t i = 0; i < config.distanceRatios.size(); ++i)
        {
            file << "ratio" << i << "=" << config.distanceRatios[i] << "\n";
        }
        file << "\n";
        
        // Blending parameters
        file << "[Blending]\n";
        file << "strength=" << config.blendingStrength << "\n";
        file << "curvePointCount=" << config.blendCurvePoints.size() << "\n";
        for (size_t i = 0; i < config.blendCurvePoints.size(); ++i)
        {
            file << "curvePoint" << i << "=" << config.blendCurvePoints[i].distanceRatio 
                 << "," << config.blendCurvePoints[i].blendMultiplier << "\n";
        }
        file << "\n";
        
        file.close();
        
        std::cout << "Configuration saved to: " << filepath << "\n";
        return true;
    }
    catch (const std::exception& e)
    {
        showError("Configuration Save Error",
                  std::string("Failed to save configuration: ") + e.what(),
                  "Check that you have write permissions and sufficient disk space.");
        return false;
    }
}

bool BakerUI::loadConfiguration(const std::string& filepath)
{
    try
    {
        std::ifstream file(filepath);
        if (!file.is_open())
        {
            showError("Configuration Load Error",
                      "Failed to open file for reading: " + filepath,
                      "Check that the file exists and you have read permissions.");
            return false;
        }
        
        // Temporary configuration to load into
        BakingConfig tempConfig;
        std::string line;
        std::string currentSection;
        
        while (std::getline(file, line))
        {
            // Skip empty lines and comments
            if (line.empty() || line[0] == '#')
            {
                continue;
            }
            
            // Check for section headers
            if (line[0] == '[' && line.back() == ']')
            {
                currentSection = line.substr(1, line.length() - 2);
                continue;
            }
            
            // Parse key=value pairs
            size_t equalPos = line.find('=');
            if (equalPos == std::string::npos)
            {
                continue;
            }
            
            std::string key = line.substr(0, equalPos);
            std::string value = line.substr(equalPos + 1);
            
            // Parse based on current section
            if (currentSection == "Output")
            {
                if (key == "directory")
                {
                    tempConfig.outputDirectory = value;
                }
            }
            else if (currentSection == "Quality")
            {
                if (key == "resolution")
                {
                    tempConfig.resolution = std::stoi(value);
                }
            }
            else if (currentSection == "SizeRatios")
            {
                if (key == "count")
                {
                    size_t count = std::stoul(value);
                    tempConfig.sizeRatios.clear();
                    tempConfig.sizeRatios.reserve(count);
                }
                else if (key.find("ratio") == 0)
                {
                    tempConfig.sizeRatios.push_back(std::stof(value));
                }
            }
            else if (currentSection == "DistanceRatios")
            {
                if (key == "count")
                {
                    size_t count = std::stoul(value);
                    tempConfig.distanceRatios.clear();
                    tempConfig.distanceRatios.reserve(count);
                }
                else if (key.find("ratio") == 0)
                {
                    tempConfig.distanceRatios.push_back(std::stof(value));
                }
            }
            else if (currentSection == "Blending")
            {
                if (key == "strength")
                {
                    tempConfig.blendingStrength = std::stof(value);
                }
                else if (key == "curvePointCount")
                {
                    size_t count = std::stoul(value);
                    tempConfig.blendCurvePoints.clear();
                    tempConfig.blendCurvePoints.reserve(count);
                }
                else if (key.find("curvePoint") == 0)
                {
                    size_t commaPos = value.find(',');
                    if (commaPos != std::string::npos)
                    {
                        float distanceRatio = std::stof(value.substr(0, commaPos));
                        float blendMultiplier = std::stof(value.substr(commaPos + 1));
                        tempConfig.blendCurvePoints.push_back({distanceRatio, blendMultiplier});
                    }
                }
            }
        }
        
        file.close();
        
        // Validate loaded configuration
        if (tempConfig.sizeRatios.empty() || tempConfig.distanceRatios.empty())
        {
            showError("Configuration Load Error",
                      "Loaded configuration is incomplete or invalid.",
                      "The configuration file may be corrupted. Try creating a new configuration.");
            return false;
        }
        
        // Apply loaded configuration
        config = tempConfig;
        
        // Update UI buffers
        strncpy_s(outputDirBuffer, sizeof(outputDirBuffer), config.outputDirectory.c_str(), _TRUNCATE);
        
        std::cout << "Configuration loaded from: " << filepath << "\n";
        return true;
    }
    catch (const std::exception& e)
    {
        showError("Configuration Load Error",
                  std::string("Failed to load configuration: ") + e.what(),
                  "The configuration file may be corrupted or in an incompatible format.");
        return false;
    }
}

std::string BakerUI::getDefaultConfigPath() const
{
    return "baker_config.txt";
}

