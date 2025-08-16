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

void UIManager::renderGenomeEditor(CellManager& cellManager, SceneManager& sceneManager)
{
    cellManager.setCellLimit(sceneManager.getCurrentCellLimit());
    ImGui::SetNextWindowPos(ImVec2(840, 50), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    
    // Set minimum window size constraints
    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 500), ImVec2(FLT_MAX, FLT_MAX));
    int flags = windowsLocked ? getWindowFlags() : getWindowFlags();
    if (ImGui::Begin("Genome Editor", nullptr, flags))
    {
        // Validate and fix any colors that might be in the wrong range
        validateGenomeColors();
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
            return false; }, &currentGenome, static_cast<int>(currentGenome.modes.size())))
    {
        // Initial mode changed
        genomeChanged = true;
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
        newMode.childA.modeNumber = static_cast<int>(currentGenome.modes.size());
        newMode.childB.modeNumber = static_cast<int>(currentGenome.modes.size());
        currentGenome.modes.push_back(newMode);
        genomeChanged = true;
    }
    addTooltip("Add a new mode to the genome");
    
    ImGui::SameLine();    if (ImGui::Button("Remove Mode") && currentGenome.modes.size() > 1)
    {
        if (selectedModeIndex >= 0 && selectedModeIndex < currentGenome.modes.size())
        {
            currentGenome.modes.erase(currentGenome.modes.begin() + selectedModeIndex);
            if (selectedModeIndex >= currentGenome.modes.size())
                selectedModeIndex = static_cast<int>(currentGenome.modes.size()) - 1;
            genomeChanged = true;
        }
    }
    addTooltip("Remove the currently selected mode from the genome");

    // Mode List
    ImGui::BeginChild("ModeList", ImVec2(200, -1), true);
    for (int i = 0; i < static_cast<int>(currentGenome.modes.size()); i++)
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
    {        ImGui::BeginChild("ModeSettings", ImVec2(0, 0), false);
        drawModeSettings(currentGenome.modes[selectedModeIndex], selectedModeIndex, cellManager);
        ImGui::EndChild();
    }    // Handle genome changes - trigger instant resimulation
    if (genomeChanged)
    {
        // Invalidate keyframes when genome changes
        keyframesInitialized = false;
        
        // Reset the simulation with the new genome
        cellManager.resetSimulation();
        cellManager.addGenomeToBuffer(currentGenome);
        ComputeCell newCell{};
        newCell.modeIndex = currentGenome.initialMode;
        // Set initial cell orientation to the genome's initial orientation
        // This keeps the initial cell orientation independent of Child A/B settings
        newCell.orientation = currentGenome.initialOrientation;
        cellManager.addCellToStagingBuffer(newCell);
        cellManager.addStagedCellsToQueueBuffer(); // Force immediate GPU buffer sync
        
        // Reset simulation time
        sceneManager.resetPreviewSimulationTime();
        
        // If time scrubber is at a specific time, fast-forward to that time
        if (currentTime > 0.0f)
        {
            // Temporarily pause to prevent normal time updates during fast-forward
            bool wasPaused = sceneManager.isPaused();
            sceneManager.setPaused(true);
            
            // Use a coarser time step for scrubbing to make it more responsive
            float scrubTimeStep = config::scrubTimeStep;
            float timeRemaining = currentTime;
            int maxSteps = (int)(currentTime / scrubTimeStep) + 1;
            
            for (int i = 0; i < maxSteps && timeRemaining > 0.0f; ++i)
            {
                float stepTime = (timeRemaining > scrubTimeStep) ? scrubTimeStep : timeRemaining;
                cellManager.updateCells(stepTime);
                timeRemaining -= stepTime;
                
                // Update simulation time manually during fast-forward
                sceneManager.setPreviewSimulationTime(currentTime - timeRemaining);
            }
            
            // Restore original pause state after fast-forward
            sceneManager.setPaused(wasPaused);
        }
        else
        {
            // If at time 0, just advance simulation by one frame after reset
            cellManager.updateCells(config::physicsTimeStep);
        }
        
        // Clear the flag
        genomeChanged = false;
        
        std::cout << "Genome changed - triggered instant resimulation to time " << currentTime << "s\n";
    }

    }
    ImGui::End();
}

void UIManager::drawModeSettings(ModeSettings &mode, int modeIndex, CellManager& cellManager)
{
    // Persistent arrays for delta sliders (per mode)
    static std::vector<float> lastPitchA, lastYawA, lastRollA;
    static std::vector<float> lastPitchB, lastYawB, lastRollB;
    if (lastPitchA.size() != currentGenome.modes.size()) {
        lastPitchA.assign(currentGenome.modes.size(), 0.0f);
        lastYawA = lastPitchA;
        lastRollA = lastPitchA;
        lastPitchB = lastPitchA;
        lastYawB = lastPitchA;
        lastRollB = lastPitchA;
    }
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
            ImGui::PushID(modeIndex * 2); // Unique ID for Child A
            // Move mode selection dropdown to the top
            drawChildSettings("Child A", mode.childA);
            // Pitch
            ImGui::Text("Pitch");
            float newP = lastPitchA[modeIndex];
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
            bool pitchChanged = ImGui::SliderFloat("##PitchSlider", &newP, -180.0f, 180.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f);
            if (ImGui::InputFloat("##PitchInput", &newP, 1.0f, 10.0f, "%.0f")) pitchChanged = true;
            ImGui::PopItemWidth();
            newP = std::round(newP); // enforce integer
            if (pitchChanged) {
                float delta = newP - lastPitchA[modeIndex];
                applyLocalRotation(mode.childA.orientation, glm::vec3(1,0,0), delta);
                lastPitchA[modeIndex] = newP;
                genomeChanged = true;
            }
            // Yaw
            ImGui::Text("Yaw");
            float newY = lastYawA[modeIndex];
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
            bool yawChanged = ImGui::SliderFloat("##YawSlider", &newY, -180.0f, 180.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f);
            if (ImGui::InputFloat("##YawInput", &newY, 1.0f, 10.0f, "%.0f")) yawChanged = true;
            ImGui::PopItemWidth();
            newY = std::round(newY);
            if (yawChanged) {
                float delta = newY - lastYawA[modeIndex];
                applyLocalRotation(mode.childA.orientation, glm::vec3(0,1,0), delta);
                lastYawA[modeIndex] = newY;
                genomeChanged = true;
            }
            // Roll
            ImGui::Text("Roll");
            float newR = lastRollA[modeIndex];
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
            bool rollChanged = ImGui::SliderFloat("##RollSlider", &newR, -180.0f, 180.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f);
            if (ImGui::InputFloat("##RollInput", &newR, 1.0f, 10.0f, "%.0f")) rollChanged = true;
            ImGui::PopItemWidth();
            newR = std::round(newR);
            if (rollChanged) {
                float delta = newR - lastRollA[modeIndex];
                applyLocalRotation(mode.childA.orientation, glm::vec3(0,0,1), delta);
                lastRollA[modeIndex] = newR;
                genomeChanged = true;
            }
            // Reset Orientation Button
            if (ImGui::Button("Reset Orientation (Child A)")) {
                mode.childA.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Reset to identity
                lastPitchA[modeIndex] = 0.0f;
                lastYawA[modeIndex] = 0.0f;
                lastRollA[modeIndex] = 0.0f;
                genomeChanged = true;
            }
            addTooltip("Snap Child A orientation to the default (identity) orientation");
            
            ImGui::PopID();
            ImGui::Separator();
            // ...existing code for Child A settings (except orientation)...
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Child B Settings"))
        {
            ImGui::PushID(modeIndex * 2 + 1); // Unique ID for Child B
            // Move mode selection dropdown to the top
            drawChildSettings("Child B", mode.childB);
            // Pitch
            ImGui::Text("Pitch");
            float newP = lastPitchB[modeIndex];
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
            bool pitchChanged = ImGui::SliderFloat("##PitchSliderB", &newP, -180.0f, 180.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f);
            if (ImGui::InputFloat("##PitchInputB", &newP, 1.0f, 10.0f, "%.0f")) pitchChanged = true;
            ImGui::PopItemWidth();
            newP = std::round(newP);
            if (pitchChanged) {
                float delta = newP - lastPitchB[modeIndex];
                applyLocalRotation(mode.childB.orientation, glm::vec3(1,0,0), delta);
                lastPitchB[modeIndex] = newP;
                genomeChanged = true;
            }
            // Yaw
            ImGui::Text("Yaw");
            float newY = lastYawB[modeIndex];
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
            bool yawChanged = ImGui::SliderFloat("##YawSliderB", &newY, -180.0f, 180.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f);
            if (ImGui::InputFloat("##YawInputB", &newY, 1.0f, 10.0f, "%.0f")) yawChanged = true;
            ImGui::PopItemWidth();
            newY = std::round(newY);
            if (yawChanged) {
                float delta = newY - lastYawB[modeIndex];
                applyLocalRotation(mode.childB.orientation, glm::vec3(0,1,0), delta);
                lastYawB[modeIndex] = newY;
                genomeChanged = true;
            }
            // Roll
            ImGui::Text("Roll");
            float newR = lastRollB[modeIndex];
            ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 100.0f);
            bool rollChanged = ImGui::SliderFloat("##RollSliderB", &newR, -180.0f, 180.0f, "%.0f");
            ImGui::PopItemWidth();
            ImGui::SameLine();
            ImGui::PushItemWidth(90.0f);
            if (ImGui::InputFloat("##RollInputB", &newR, 1.0f, 10.0f, "%.0f")) rollChanged = true;
            ImGui::PopItemWidth();
            newR = std::round(newR);
            if (rollChanged) {
                float delta = newR - lastRollB[modeIndex];
                applyLocalRotation(mode.childB.orientation, glm::vec3(0,0,1), delta);
                lastRollB[modeIndex] = newR;
                genomeChanged = true;
            }
            // Reset Orientation Button
            if (ImGui::Button("Reset Orientation (Child B)")) {
                mode.childB.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Reset to identity
                lastPitchB[modeIndex] = 0.0f;
                lastYawB[modeIndex] = 0.0f;
                lastRollB[modeIndex] = 0.0f;
                genomeChanged = true;
            }
            addTooltip("Snap Child B orientation to the default (identity) orientation");
            
            ImGui::PopID();
            ImGui::Separator();
            // ...existing code for Child B settings (except orientation)...
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
                drawAdhesionSettings(mode.adhesionSettings);
            }
            else
            {
                ImGui::TextDisabled("Enable 'Parent Make Adhesion' to configure adhesionSettings settings");
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
    // Mode Name
    ImGui::Text("Mode Name:");
    addTooltip("The name of this mode (used for identification in the UI)");
    static char nameBuffer[256];
    static int lastModeIndex = -1;
    
    // Update buffer when mode changes
    if (lastModeIndex != selectedModeIndex)
    {
        lastModeIndex = selectedModeIndex;
        mode.name.copy(nameBuffer, sizeof(nameBuffer) - 1);
        nameBuffer[std::min(mode.name.length(), sizeof(nameBuffer) - 1)] = '\0';
    }
    
    if (ImGui::InputText("##ModeName", nameBuffer, sizeof(nameBuffer)))
    {
        mode.name = std::string(nameBuffer);
        genomeChanged = true;
    }

    // Add divider before color picker
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Color Picker
    ImGui::Text("Mode Color:");
    addTooltip("The color of cells in this mode (used for rendering)");
    drawColorPicker("##ModeColor", &mode.color);

    // Add divider before split settings
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    drawSliderWithInput("Split Mass", &mode.splitMass, 0.1f, 10.0f, "%.2f");
    addTooltip("The mass threshold at which the cell will split into two child cells");
    
    drawSliderWithInput("Split Interval", &mode.splitInterval, 1.0f, 30.0f, "%.1f");
    addTooltip("Time interval (in seconds) between cell splits");    // Add divider before Parent Split Angle
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Parent Split Angle:");
    addTooltip("Controls the vector direction that child cells split along relative to the parent");
      drawSliderWithInput("Pitch", &mode.parentSplitDirection.x, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Vertical angle of the split vector (up/down direction for child cell placement)");
    
    drawSliderWithInput("Yaw", &mode.parentSplitDirection.y, -180.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Horizontal angle of the split vector (left/right direction for child cell placement)");

    // Add divider before Parent Make Adhesion checkbox
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
	if (ImGui::Checkbox("Parent Make Adhesion", &mode.parentMakeAdhesion))
    {
        genomeChanged = true;
    }
    addTooltip("Whether the parent cell creates adhesive connections with its children");

    drawSliderWithInput("Max Adhesions", &mode.maxAdhesions, 0, config::MAX_ADHESIONS_PER_CELL);
    addTooltip("Maximum adhesion connections. Prevents cell from splitting if the maximum would be exceeded.");
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
                         return false; }, this, static_cast<int>(currentGenome.modes.size())))
    {
        // Mode selection changed - clamp to valid range
        if (child.modeNumber >= currentGenome.modes.size())
        {
            child.modeNumber = static_cast<int>(currentGenome.modes.size()) - 1;
        }
        if (child.modeNumber < 0)
        {
            child.modeNumber = 0;
        }
        genomeChanged = true;
    }

    // Remove old orientation controls (now handled by delta-aware sliders in drawModeSettings)
    // Add some spacing before other controls
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Add divider before Keep Adhesion checkbox
    if (ImGui::Checkbox("Keep Adhesion", &child.keepAdhesion)) {
        genomeChanged = true;
    }
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
    addTooltip("How strongly the adhesionSettings resists stretching or compression");
    
    drawSliderWithInput("Linear Spring Damping", &adhesion.linearSpringDamping, 0.0f, 1.0f);
    addTooltip("Damping factor that reduces oscillations in the adhesive connection");

    drawSliderWithInput("Angular Spring Stiffness", &adhesion.orientationSpringStiffness, 0.1f, 30.0f);
    addTooltip("How strongly the adhesionSettings resists rotational changes between connected cells");

    drawSliderWithInput("Angular Spring Damping", &adhesion.orientationSpringDamping, 0.0f, 1.0f);
    addTooltip("Damping factor that reduces oscillations in the adhesive connection");
    
    drawSliderWithInput("Max Angular Deviation", &adhesion.maxAngularDeviation, 0.0f, 180.0f, "%.0f°", 1.0f);
    addTooltip("Maximum angle between connected cells (0° = strict orientation locking, >0° = flexible with max deviation)");
}