#include "ui_manager.h"
#include "cell_manager.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>

#include "audio_engine.h"
#include "scene_manager.h"

// Ensure std::min and std::max are available
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

void UIManager::renderCellInspector(CellManager &cellManager, SceneManager& sceneManager)
{
    ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 400), ImGuiCond_FirstUseEver);
      int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Cell Inspector", nullptr, flags))
    {

    if (cellManager.hasSelectedCell())
    {
    	const auto &selectedCell = cellManager.getSelectedCell();
        ImGui::Text("Selected Cell #%d", selectedCell.cellIndex);
        ImGui::Separator();

        // Display current properties
        glm::vec3 position = glm::vec3(selectedCell.cellData.positionAndMass);
        glm::vec3 velocity = glm::vec3(selectedCell.cellData.velocity);
        float mass = selectedCell.cellData.positionAndMass.w;
        //float radius = selectedCell.cellData.getRadius();
        int modeIndex = selectedCell.cellData.modeIndex;
        float age = selectedCell.cellData.age;

        ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity.x, velocity.y, velocity.z);
        ImGui::Text("Mass: %.2f", mass);
        //ImGui::Text("Radius: %.2f", radius);
        ImGui::Text("Absolute Mode Index: %i", modeIndex);
        ImGui::Text("Age: %.2f", age);

        ImGui::Separator();

        // Editable properties
        ImGui::Text("Edit Properties:");

        bool changed = false;
        ComputeCell editedCell = selectedCell.cellData;

        // Position editing
        float pos[3] = {position.x, position.y, position.z};
        if (ImGui::DragFloat3("Position", pos, 0.1f))
        {
            editedCell.positionAndMass.x = pos[0];
            editedCell.positionAndMass.y = pos[1];
            editedCell.positionAndMass.z = pos[2];
            changed = true;
        }

        // Velocity editing
        float vel[3] = {velocity.x, velocity.y, velocity.z};
        if (ImGui::DragFloat3("Velocity", vel, 0.1f))
        {
            editedCell.velocity.x = vel[0];
            editedCell.velocity.y = vel[1];
            editedCell.velocity.z = vel[2];
            changed = true;
        }

        // Mass editing
        if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.1f, 50.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
        {
            editedCell.positionAndMass.w = mass;
            changed = true;
        }

        // Apply changes
        if (changed)
        {
            cellManager.updateCellData(selectedCell.cellIndex, editedCell);
        }

        ImGui::Separator();

        // Action buttons
        if (ImGui::Button("Clear Selection"))
        {
            cellManager.clearSelection();
        }
        // Status
        if (cellManager.isDraggingCell)
        {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "DRAGGING");
            ImGui::Text("Drag Distance: %.2f", selectedCell.dragDistance);
            ImGui::Text("(Use scroll wheel to adjust distance)");
        }
    }
    else
    {
        ImGui::Text("No cell selected");
        ImGui::Separator();
        ImGui::Text("Instructions:");
        ImGui::BulletText("Left-click on a cell to select it");
        ImGui::BulletText("Drag to move selected cell");
        ImGui::BulletText("Scroll wheel to adjust distance");        ImGui::BulletText("Selected cell moves in a plane");
        ImGui::BulletText("parallel to the camera");
    }

    }
    ImGui::End();
}

void UIManager::drawToolSelector(ToolState &toolState)
{
    const char *tools[] = {"None", "Add", "Edit", "Move (UNIMPLEMENTED)"};
    int current = static_cast<int>(toolState.activeTool);
    if (ImGui::Combo("Tool", &current, tools, IM_ARRAYSIZE(tools)))
    {
        toolState.activeTool = static_cast<ToolType>(current);
    }
}

void UIManager::drawToolSettings(ToolState &toolState, CellManager &cellManager)
{
    switch (toolState.activeTool)
    {
    case ToolType::AddCell:
        ImGui::ColorEdit4("New Cell Color", &toolState.newCellColor[0]);
        ImGui::SliderFloat("New Cell Mass", &toolState.newCellMass, 0.1f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        break;
    case ToolType::EditCell:

    default:
        break;
    }
}

void UIManager::renderPerformanceMonitor(CellManager &cellManager, PerformanceMonitor &perfMonitor, SceneManager& sceneManager)
{
    ImGui::SetNextWindowPos(ImVec2(420, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
      int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Advanced Performance Monitor", nullptr, flags))
    {

    // === FPS and Frame Time Section ===
    ImGui::Text("Performance Overview");
    ImGui::Separator();

    // Main metrics with color coding
    ImGui::Text("FPS: ");
    ImGui::SameLine();
    ImVec4 fpsColor = perfMonitor.displayFPS >= 60.0f ? ImVec4(0, 1, 0, 1) : perfMonitor.displayFPS >= 30.0f ? ImVec4(1, 1, 0, 1)
                                                                                                             : ImVec4(1, 0, 0, 1);
    ImGui::TextColored(fpsColor, "%.1f", perfMonitor.displayFPS);

    ImGui::Text("Frame Time: ");
    ImGui::SameLine();
    ImVec4 frameTimeColor = perfMonitor.displayFrameTime <= 16.67f ? ImVec4(0, 1, 0, 1) : perfMonitor.displayFrameTime <= 33.33f ? ImVec4(1, 1, 0, 1)
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
                         perfMonitor.frameTimeHistory.size(), 0, nullptr,
                         0.0f, 50.0f, ImVec2(0, 80));
    }

    ImGui::Text("FPS History");
    if (!perfMonitor.fpsHistory.empty())
    {
        ImGui::PlotLines("##FPS", perfMonitor.fpsHistory.data(),
                         perfMonitor.fpsHistory.size(), 0, nullptr,
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
    if (perfMonitor.displayFPS >= 60.0f)
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
    if (perfMonitor.displayFPS >= 60.0f)
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
    if (perfMonitor.displayFrameTime <= 16.67f)
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
    ImGui::Text("Active Cells: %i / %i", cellCount, config::MAX_CELLS);
    ImGui::Text("Pending Cells: CPU: %i, GPU: %i", cellManager.cpuPendingCellCount, cellManager.gpuPendingCellCount);
    ImGui::Text("Triangles: ~%i", 192 * cellCount);
    ImGui::Text("Vertices: ~%i", 96 * cellCount); // Assuming sphere has ~96 vertices

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

void UIManager::renderCameraControls(CellManager &cellManager, Camera &camera, SceneManager& sceneManager)
{
    ImGui::SetNextWindowPos(ImVec2(50, 470), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(350, 200), ImGuiCond_FirstUseEver);
      int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Camera & Controls", nullptr, flags))
    {
    glm::vec3 camPos = camera.getPosition();
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
    ImGui::Separator();
    
    // Window lock/unlock button
    ImGui::Text("Window Management:");
    if (ImGui::Button(windowsLocked ? "Unlock All Windows" : "Lock All Windows"))
    {
        windowsLocked = !windowsLocked;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Lock/unlock position and size of all UI windows");
    }
    ImGui::Separator();
    
    ImGui::Text("Camera Controls:");
    ImGui::BulletText("WASD - Move");
    ImGui::BulletText("Q/E - Roll");
    ImGui::BulletText("Space/C - Up/Down");
    ImGui::BulletText("Right-click + Drag - Look");
    ImGui::Separator();
    ImGui::Text("Cell Interaction:");
    ImGui::BulletText("Left-click - Select cell");
    ImGui::BulletText("Left-click + Drag - Move selected cell");
    ImGui::BulletText("Scroll Wheel - Adjust drag distance");

    // Show current selection info
    if (cellManager.hasSelectedCell())    {
        ImGui::Separator();
        const auto &selection = cellManager.getSelectedCell();
        ImGui::Text("Selected: Cell #%d", selection.cellIndex);
        ImGui::Text("Drag Distance: %.1f", selection.dragDistance);
    }
    }
    ImGui::End();
}

// Helper method to get window flags based on lock state
int UIManager::getWindowFlags(int baseFlags) const
{
    if (windowsLocked)
    {
        // Remove AlwaysAutoResize flag if present since it conflicts with NoResize
        int lockedFlags = baseFlags & ~ImGuiWindowFlags_AlwaysAutoResize;
        return lockedFlags | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize;
    }
    return baseFlags;
}

void UIManager::renderGenomeEditor(SceneManager& sceneManager)
{
    ImGui::SetNextWindowPos(ImVec2(840, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    
    // Set minimum window size constraints
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 500), ImVec2(FLT_MAX, FLT_MAX));    int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Genome Editor", nullptr, flags))
    {
    ImGui::Text("Genome Name:");
    addTooltip("The name identifier for this genome configuration");
    
    ImGui::SameLine();
    char nameBuffer[256];
    strcpy_s(nameBuffer, currentGenome.name.c_str());
    ImGui::PushItemWidth(200.0f);
    if (ImGui::InputText("##GenomeName", nameBuffer, sizeof(nameBuffer)))
    {
        currentGenome.name = std::string(nameBuffer);
    }
    ImGui::PopItemWidth();

    ImGui::SameLine();
    if (ImGui::Button("Save Genome"))
    {
        // TODO: Implement genome saving functionality
        ImGui::OpenPopup("Save Confirmation");
    }
    addTooltip("Save the current genome configuration to file");

    ImGui::SameLine();
    if (ImGui::Button("Load Genome"))
    {
        // TODO: Implement genome loading functionality
        ImGui::OpenPopup("Load Confirmation");
    }
    addTooltip("Load a previously saved genome configuration");

    // Save confirmation popup
    if (ImGui::BeginPopupModal("Save Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Genome '%s' saved successfully!", currentGenome.name.c_str());
        ImGui::Text("(Save functionality not yet implemented)");
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    // Load confirmation popup
    if (ImGui::BeginPopupModal("Load Confirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Load genome functionality not yet implemented.");
        if (ImGui::Button("OK"))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    ImGui::Separator();    // Initial Mode Dropdown
    ImGui::Text("Initial Mode:");
    addTooltip("The starting mode for new cells in this genome");
    
    ImGui::SameLine();
    if (ImGui::Combo("##InitialMode", &currentGenome.initialMode, [](void *data, int idx, const char **out_text) -> bool
                     {
            GenomeData* genome = (GenomeData*)data;
            if (idx >= 0 && idx < genome->modes.size()) {
                *out_text = genome->modes[idx].name.c_str();
                return true;
            }
            return false; }, &currentGenome, currentGenome.modes.size()))
    {
        // Initial mode changed
    }

    ImGui::Separator();

    // Mode Management
    ImGui::Text("Modes:");
    addTooltip("Manage the different behavioral modes available in this genome");
    
    ImGui::SameLine();
    if (ImGui::Button("Add Mode"))
    {
        ModeSettings newMode;
        newMode.name = "Mode " + std::to_string(currentGenome.modes.size());
        currentGenome.modes.push_back(newMode);
    }
    addTooltip("Add a new mode to the genome");
    
    ImGui::SameLine();
    if (ImGui::Button("Remove Mode") && currentGenome.modes.size() > 1)
    {
        if (selectedModeIndex >= 0 && selectedModeIndex < currentGenome.modes.size())
        {
            currentGenome.modes.erase(currentGenome.modes.begin() + selectedModeIndex);
            if (selectedModeIndex >= currentGenome.modes.size())
                selectedModeIndex = currentGenome.modes.size() - 1;
        }
    }
    addTooltip("Remove the currently selected mode from the genome");

    // Mode List
    ImGui::BeginChild("ModeList", ImVec2(200, -1), true);
    for (int i = 0; i < currentGenome.modes.size(); i++)
    {
        // Color the mode tab with the mode's color - keep bright for unselected modes
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImVec4(currentGenome.modes[i].color.r * 0.8f,
                                     currentGenome.modes[i].color.g * 0.8f,
                                     currentGenome.modes[i].color.b * 0.8f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImVec4(currentGenome.modes[i].color.r * 0.9f,
                                     currentGenome.modes[i].color.g * 0.9f,
                                     currentGenome.modes[i].color.b * 0.9f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImVec4(currentGenome.modes[i].color.r,
                                     currentGenome.modes[i].color.g,
                                     currentGenome.modes[i].color.b, 1.0f));
        bool isSelected = (i == selectedModeIndex);
        if (isSelected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(currentGenome.modes[i].color.r,
                                         currentGenome.modes[i].color.g,
                                         currentGenome.modes[i].color.b, 1.0f));
        }

        // Set text color based on brightness of the mode color
        glm::vec3 buttonColor = isSelected ? currentGenome.modes[i].color : currentGenome.modes[i].color * 0.8f;
        if (isColorBright(buttonColor))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); // White text
        }
        std::string buttonLabel = std::to_string(i) + ": " + currentGenome.modes[i].name;
        if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1, 0)))
        {
            selectedModeIndex = i;
        } // Draw outline for selected mode (red or white depending on background)
        if (isSelected)
        {
            ImDrawList *draw_list = ImGui::GetWindowDrawList();
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();

            // Create alternating black and white outline by drawing dashed lines
            float dashLength = 6.0f;
            ImU32 blackColor = IM_COL32(0, 0, 0, 255);
            ImU32 whiteColor = IM_COL32(255, 255, 255, 255);

            // Draw top edge
            for (float x = min.x; x < max.x; x += dashLength * 2)
            {
                float endX = std::min(x + dashLength, max.x);
                draw_list->AddLine(ImVec2(x, min.y), ImVec2(endX, min.y), blackColor, 2.0f);
                endX = std::min(x + dashLength * 2, max.x);
                if (x + dashLength < max.x)
                    draw_list->AddLine(ImVec2(x + dashLength, min.y), ImVec2(endX, min.y), whiteColor, 2.0f);
            }

            // Draw bottom edge
            for (float x = min.x; x < max.x; x += dashLength * 2)
            {
                float endX = std::min(x + dashLength, max.x);
                draw_list->AddLine(ImVec2(x, max.y), ImVec2(endX, max.y), blackColor, 2.0f);
                endX = std::min(x + dashLength * 2, max.x);
                if (x + dashLength < max.x)
                    draw_list->AddLine(ImVec2(x + dashLength, max.y), ImVec2(endX, max.y), whiteColor, 2.0f);
            }

            // Draw left edge
            for (float y = min.y; y < max.y; y += dashLength * 2)
            {
                float endY = std::min(y + dashLength, max.y);
                draw_list->AddLine(ImVec2(min.x, y), ImVec2(min.x, endY), blackColor, 2.0f);
                endY = std::min(y + dashLength * 2, max.y);
                if (y + dashLength < max.y)
                    draw_list->AddLine(ImVec2(min.x, y + dashLength), ImVec2(min.x, endY), whiteColor, 2.0f);
            }

            // Draw right edge
            for (float y = min.y; y < max.y; y += dashLength * 2)
            {
                float endY = std::min(y + dashLength, max.y);
                draw_list->AddLine(ImVec2(max.x, y), ImVec2(max.x, endY), blackColor, 2.0f);
                endY = std::min(y + dashLength * 2, max.y);
                if (y + dashLength < max.y)
                    draw_list->AddLine(ImVec2(max.x, y + dashLength), ImVec2(max.x, endY), whiteColor, 2.0f);
            }
        }

        ImGui::PopStyleColor(isSelected ? 5 : 4); // Pop text color + button colors
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // Mode Settings Panel
    if (selectedModeIndex >= 0 && selectedModeIndex < currentGenome.modes.size())
    {
        ImGui::BeginChild("ModeSettings", ImVec2(0, 0), false);        drawModeSettings(currentGenome.modes[selectedModeIndex], selectedModeIndex);
        ImGui::EndChild();
    }

    }
    ImGui::End();
}

void UIManager::drawModeSettings(ModeSettings &mode, int modeIndex)
{
    ImGui::Text("Mode %d Settings", modeIndex);
    ImGui::Separator();    // Mode Name
    char nameBuffer[256];
    strcpy_s(nameBuffer, mode.name.c_str());
    if (ImGui::InputText("Mode Name", nameBuffer, sizeof(nameBuffer)))
    {
        mode.name = std::string(nameBuffer);
    }
    addTooltip("The display name for this cell mode");

    // Mode Color
    drawColorPicker("Mode Color", &mode.color);
    addTooltip("The visual color representing this cell mode");

    ImGui::Separator();

    // Tabbed interface for different settings
    if (ImGui::BeginTabBar("ModeSettingsTabs"))
    {
        if (ImGui::BeginTabItem("Parent Settings"))
        {
            drawParentSettings(mode);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Child A Settings"))
        {
            drawChildSettings("Child A", mode.childA);
            ImGui::EndTabItem();
        }        if (ImGui::BeginTabItem("Child B Settings"))
        {
            drawChildSettings("Child B", mode.childB);
            ImGui::EndTabItem();
        }

        // Grey out Adhesion Settings tab when Parent Make Adhesion is not checked
        bool adhesionTabEnabled = mode.parentMakeAdhesion;
        if (!adhesionTabEnabled)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
        }
        
        if (ImGui::BeginTabItem("Adhesion Settings", nullptr, adhesionTabEnabled ? ImGuiTabItemFlags_None : ImGuiTabItemFlags_NoTooltip))
        {
            if (adhesionTabEnabled)
            {
                drawAdhesionSettings(mode.adhesion);
            }
            else
            {
                ImGui::TextDisabled("Enable 'Parent Make Adhesion' to configure adhesion settings");
            }
            ImGui::EndTabItem();
        }
        
        if (!adhesionTabEnabled)
        {
            ImGui::PopStyleVar();
        }

        ImGui::EndTabBar();
    }
}

void UIManager::drawParentSettings(ModeSettings &mode)
{
    drawSliderWithInput("Split Mass", &mode.splitMass, 0.1f, 10.0f, "%.2f");
    addTooltip("The mass threshold at which the cell will split into two child cells");
    
    drawSliderWithInput("Split Interval", &mode.splitInterval, 1.0f, 30.0f, "%.1f");
    addTooltip("Time interval (in seconds) between cell splits");    // Add divider before Parent Split Angle
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Parent Split Angle:");
    addTooltip("Controls the vector direction that child cells split along relative to the parent");
      drawSliderWithInput("Pitch", &mode.parentSplitOrientation.x, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Vertical angle of the split vector (up/down direction for child cell placement)");
    
    drawSliderWithInput("Yaw", &mode.parentSplitOrientation.y, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Horizontal angle of the split vector (left/right direction for child cell placement)");

    // Add divider before Parent Make Adhesion checkbox
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Parent Make Adhesion", &mode.parentMakeAdhesion);
    addTooltip("Whether the parent cell creates adhesive connections with its children");
}

void UIManager::drawChildSettings(const char *label, ChildSettings &child)
{
    // Mode selection dropdown
    ImGui::Text("Mode:");
    addTooltip("The cell mode that this child will switch to after splitting");
    
    if (ImGui::Combo("##Mode", &child.modeNumber, [](void *data, int idx, const char **out_text) -> bool
                     {
                         UIManager* uiManager = (UIManager*)data;
                         if (idx >= 0 && idx < uiManager->currentGenome.modes.size()) {
                             *out_text = uiManager->currentGenome.modes[idx].name.c_str();
                             return true;
                         }
                         return false; }, this, currentGenome.modes.size()))
    {
        // Mode selection changed - clamp to valid range
        if (child.modeNumber >= currentGenome.modes.size())
        {
            child.modeNumber = currentGenome.modes.size() - 1;
        }
        if (child.modeNumber < 0)
        {
            child.modeNumber = 0;
        }
    }

    // Add some spacing before orientation controls
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Orientation:");
    addTooltip("The initial orientation angles of the child cell after splitting");
    
    drawSliderWithInput("Pitch", &child.orientation.x, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Rotation around the X-axis (up/down angle) for the child's initial orientation");
    
    drawSliderWithInput("Yaw", &child.orientation.y, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Rotation around the Y-axis (left/right angle) for the child's initial orientation");
    
    drawSliderWithInput("Roll", &child.orientation.z, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Rotation around the Z-axis (twist angle) for the child's initial orientation");

    // Add divider before Keep Adhesion checkbox
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Checkbox("Keep Adhesion", &child.keepAdhesion);
    addTooltip("Whether this child maintains adhesive connections with its parent and siblings");
}

void UIManager::drawAdhesionSettings(AdhesionSettings &adhesion)
{
    ImGui::Checkbox("Adhesion Can Break", &adhesion.canBreak);
    addTooltip("Whether adhesive connections can be broken by external forces");
    
    drawSliderWithInput("Adhesion Break Force", &adhesion.breakForce, 0.1f, 100.0f);
    addTooltip("The force threshold required to break an adhesive connection");
    
    drawSliderWithInput("Adhesion Rest Length", &adhesion.restLength, 0.1f, 10.0f);
    addTooltip("The natural resting distance of the adhesive connection");
    
    drawSliderWithInput("Linear Spring Stiffness", &adhesion.linearSpringStiffness, 0.1f, 50.0f);
    addTooltip("How strongly the adhesion resists stretching or compression");
    
    drawSliderWithInput("Linear Spring Damping", &adhesion.linearSpringDamping, 0.0f, 5.0f);
    addTooltip("Damping factor that reduces oscillations in the adhesive connection");    drawSliderWithInput("Angular Spring Stiffness", &adhesion.orientationSpringStrength, 0.1f, 20.0f);
    addTooltip("How strongly the adhesion resists rotational changes between connected cells");
    
    drawSliderWithInput("Max Angular Deviation", &adhesion.maxAngularDeviation, 0.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("How far the adhesive connection can bend freely before angular constraints kick in");
}

void UIManager::addTooltip(const char* tooltip)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("%s", tooltip);
    }
}

void UIManager::drawSliderWithInput(const char *label, float *value, float min, float max, const char *format, float step)
{
    ImGui::PushID(label);     // Calculate layout with proper spacing
    float inputWidth = 80.0f; // Increased input field width
    float availableWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;

    // Calculate slider width: slider + input field
    float sliderWidth = availableWidth - inputWidth - spacing;

    // Label on its own line
    ImGui::Text("%s", label); // Slider with calculated width and step support
    ImGui::PushItemWidth(sliderWidth);
    if (step > 0.0f)
    {
        // For stepped sliders, use float slider but with restricted stepping
        if (ImGui::SliderFloat("##slider", value, min, max, format))
        {
            // Round to nearest step
            *value = min + step * round((*value - min) / step);
        }
    }
    else
    {
        // Use regular float slider for continuous values
        ImGui::SliderFloat("##slider", value, min, max, format);
    }
    ImGui::PopItemWidth();
    ImGui::SameLine();

    // Input field with proper width
    ImGui::PushItemWidth(inputWidth);
    if (step > 0.0f)
    {
        float stepValue = step;
        if (ImGui::InputFloat("##input", value, stepValue, stepValue, format))
        {
            // Round to nearest step
            *value = min + step * round((*value - min) / step);
        }
    }
    else
    {
        ImGui::InputFloat("##input", value, 0.0f, 0.0f, format);
    }
    ImGui::PopItemWidth();

    // Clamp value to range
    if (*value < min)
        *value = min;
    if (*value > max)
        *value = max;

    ImGui::PopID();
}

void UIManager::drawColorPicker(const char *label, glm::vec3 *color)
{
    float colorArray[3] = {color->r, color->g, color->b};
    if (ImGui::ColorEdit3(label, colorArray))
    {
        color->r = colorArray[0];
        color->g = colorArray[1];
        color->b = colorArray[2];
    }
}

bool UIManager::isColorBright(const glm::vec3 &color)
{
    // Calculate luminance using standard weights for RGB
    // This is the perceived brightness formula
    float luminance = 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
    return luminance > 0.5f; // Threshold for considering a color "bright"
}

void UIManager::updatePerformanceMetrics(PerformanceMonitor &perfMonitor, float deltaTime)
{
    // Update frame time statistics
    float frameTimeMs = deltaTime * 1000.0f;

    if (frameTimeMs < perfMonitor.minFrameTime)
        perfMonitor.minFrameTime = frameTimeMs;
    if (frameTimeMs > perfMonitor.maxFrameTime)
        perfMonitor.maxFrameTime = frameTimeMs;

    // Update frame time history
    perfMonitor.frameTimeHistory.push_back(frameTimeMs);
    if (perfMonitor.frameTimeHistory.size() > PerformanceMonitor::HISTORY_SIZE)
        perfMonitor.frameTimeHistory.erase(perfMonitor.frameTimeHistory.begin());

    // Update FPS history
    float currentFPS = deltaTime > 0.0f ? 1.0f / deltaTime : 0.0f;
    perfMonitor.fpsHistory.push_back(currentFPS);
    if (perfMonitor.fpsHistory.size() > PerformanceMonitor::HISTORY_SIZE)
        perfMonitor.fpsHistory.erase(perfMonitor.fpsHistory.begin());

    // Calculate average frame time
    if (!perfMonitor.frameTimeHistory.empty())
    {
        float sum = 0.0f;
        for (float ft : perfMonitor.frameTimeHistory)
            sum += ft;
        perfMonitor.avgFrameTime = sum / perfMonitor.frameTimeHistory.size();
    }

    // Reset min/max periodically (every 5 seconds)
    static float resetTimer = 0.0f;
    resetTimer += deltaTime;
    if (resetTimer >= 5.0f)
    {
        perfMonitor.minFrameTime = 1000.0f;
        perfMonitor.maxFrameTime = 0.0f;
        resetTimer = 0.0f;
    }
}

void UIManager::renderTimeScrubber(CellManager& cellManager, SceneManager& sceneManager)
{
    // Set window size and position for a long horizontal resizable window
    ImGui::SetNextWindowPos(ImVec2(50, 680), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 120), ImGuiCond_FirstUseEver);
      int flags = windowsLocked ? getWindowFlags(ImGuiWindowFlags_None) : getWindowFlags(ImGuiWindowFlags_None);
    if (ImGui::Begin("Time Scrubber", nullptr, flags))
    {
        // Get available width for responsive layout
        float available_width = ImGui::GetContentRegionAvail().x;          // Title and main slider on one line
        ImGui::Text("Time Scrubber");
        
        // Calculate available width for slider (reserve space for input field)
        float input_width = 80.0f;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float slider_width = available_width - input_width - spacing;
        
        // Make the slider take almost all available width
        ImGui::SetNextItemWidth(slider_width);
        if (ImGui::SliderFloat("##TimeSlider", &currentTime, 0.0f, maxTime, "%.2f"))
        {
            // Update input buffer when slider changes
            snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
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
        if (ImGui::DragFloat("##MaxTime", &maxTime, 1.0f, 1.0f, 10000.0f, "%.2f"))
        {
            // Ensure current time doesn't exceed max time
            if (currentTime > maxTime)
            {
                currentTime = maxTime;
                snprintf(timeInputBuffer, sizeof(timeInputBuffer), "%.2f", currentTime);
            }
        }
    }
    ImGui::End();
}

void UIManager::renderSceneSwitcher(SceneManager& sceneManager, CellManager& previewCellManager, CellManager& mainCellManager)
{
    // Set window position on first use - top center
    ImGui::SetNextWindowPos(ImVec2(400, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 300), ImGuiCond_FirstUseEver);
      int flags = windowsLocked ? getWindowFlags(ImGuiWindowFlags_None) : getWindowFlags(ImGuiWindowFlags_None);    if (ImGui::Begin("Scene Manager", nullptr, flags))
    {
        // Get current scene for use throughout the function
        Scene currentScene = sceneManager.getCurrentScene();
        
        // === CURRENT SCENE SECTION ===
        ImGui::Text("Current Scene: %s", sceneManager.getCurrentSceneName());
        ImGui::Separator();
          // === SIMULATION CONTROLS SECTION ===
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
        if (currentScene == Scene::PreviewSimulation)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f)); // Red for reset
            if (ImGui::Button("Reset Preview", ImVec2(150, 30)))
            {
                previewCellManager.resetSimulation();
                previewCellManager.addGenomeToBuffer(currentGenome);
                ComputeCell newCell{};
                newCell.modeIndex = currentGenome.initialMode;
                previewCellManager.addCellToStagingBuffer(newCell);
            }
            ImGui::PopStyleColor();
        }
        else if (currentScene == Scene::MainSimulation)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.3f, 0.3f, 1.0f)); // Red for reset
            if (ImGui::Button("Reset Main", ImVec2(150, 30)))
            {
                mainCellManager.resetSimulation();
                mainCellManager.addGenomeToBuffer(currentGenome);
                ComputeCell newCell{};
                newCell.modeIndex = currentGenome.initialMode;
                mainCellManager.addCellToStagingBuffer(newCell);
            }
            ImGui::PopStyleColor();
        }
        
        // Speed controls
        float currentSpeed = sceneManager.getSimulationSpeed();
        ImGui::Text("Speed: %.1fx", currentSpeed);
        
        // Speed slider
        if (ImGui::SliderFloat("##Speed", &currentSpeed, 0.1f, 5.0f, "%.1fx"))
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
        ImGui::Separator();        
        // Status info
        ImGui::Spacing();
        ImGui::TextDisabled("Status: %s | Speed: %.1fx", 
            isPaused ? "PAUSED" : "RUNNING", 
            sceneManager.getSimulationSpeed());
    }
    ImGui::End();
}