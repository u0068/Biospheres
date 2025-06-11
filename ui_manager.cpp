#include "ui_manager.h"
#include "cell_manager.h"
#include "genome_system.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

// Helper function to clamp values (C++17 std::clamp replacement)
template<typename T>
T clamp(const T& value, const T& min_val, const T& max_val) {
    return std::max(min_val, std::min(value, max_val));
}

void UIManager::renderCellInspector(CellManager& cellManager) {
    ImGui::Begin("Cell Inspector", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    if (cellManager.hasSelectedCell()) {
        const auto& selectedCell = cellManager.getSelectedCell();
        ImGui::Text("Selected Cell #%d", selectedCell.cellIndex);
        ImGui::Separator();
        
        // Display current properties
        glm::vec3 position = glm::vec3(selectedCell.cellData.positionAndRadius);
        glm::vec3 velocity = glm::vec3(selectedCell.cellData.velocityAndMass);
        float mass = selectedCell.cellData.velocityAndMass.w;
        float radius = selectedCell.cellData.positionAndRadius.w;
          ImGui::Text("Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z);
        ImGui::Text("Velocity: (%.2f, %.2f, %.2f)", velocity.x, velocity.y, velocity.z);
        ImGui::Text("Mass: %.2f", mass);
        ImGui::Text("Radius: %.2f", radius);
        
        // Genome data
        int modeIndex = selectedCell.cellData.genomeData.x;
        int splitTimer = selectedCell.cellData.genomeData.y;
        ImGui::Text("Genome Mode: %d", modeIndex);
        ImGui::Text("Split Timer: %d ms", splitTimer);
        
        // Show mode color if genome system is available
        if (g_genomeSystem && modeIndex >= 0 && modeIndex < static_cast<int>(g_genomeSystem->getModeCount())) {
            const GenomeMode* mode = g_genomeSystem->getMode(modeIndex);
            if (mode) {
                ImGui::Text("Mode Name: %s", mode->modeName.c_str());
                ImGui::ColorButton("Mode Color", ImVec4(mode->modeColor.r, mode->modeColor.g, mode->modeColor.b, mode->modeColor.a), 
                                 ImGuiColorEditFlags_NoTooltip, ImVec2(20, 20));
                ImGui::SameLine();
                ImGui::Text("Split Interval: %.1f sec", mode->splitInterval);
            }
        }
        
        ImGui::Separator();
        
        // Editable properties
        ImGui::Text("Edit Properties:");
        
        bool changed = false;
        ComputeCell editedCell = selectedCell.cellData;
        
        // Position editing
        float pos[3] = { position.x, position.y, position.z };
        if (ImGui::DragFloat3("Position", pos, 0.1f)) {
            editedCell.positionAndRadius.x = pos[0];
            editedCell.positionAndRadius.y = pos[1];
            editedCell.positionAndRadius.z = pos[2];
            changed = true;
        }
        
        // Velocity editing
        float vel[3] = { velocity.x, velocity.y, velocity.z };
        if (ImGui::DragFloat3("Velocity", vel, 0.1f)) {
            editedCell.velocityAndMass.x = vel[0];
            editedCell.velocityAndMass.y = vel[1];
            editedCell.velocityAndMass.z = vel[2];
            changed = true;
        }
        
        // Mass editing
        if (ImGui::DragFloat("Mass", &mass, 0.1f, 0.1f, 50.0f)) {
            editedCell.velocityAndMass.w = mass;
            changed = true;
        }
          // Radius editing
        if (ImGui::DragFloat("Radius", &radius, 0.1f, 0.1f, 5.0f)) {
            editedCell.positionAndRadius.w = radius;
            changed = true;
        }
        
        // Genome mode editing
        if (g_genomeSystem && g_genomeSystem->getModeCount() > 0) {
            int currentModeIndex = editedCell.genomeData.x;
            int maxModes = static_cast<int>(g_genomeSystem->getModeCount());
            
            if (ImGui::SliderInt("Genome Mode", &currentModeIndex, 0, maxModes - 1)) {
                editedCell.genomeData.x = currentModeIndex;
                changed = true;
            }
            
            // Show mode name
            if (currentModeIndex >= 0 && currentModeIndex < maxModes) {
                const GenomeMode* mode = g_genomeSystem->getMode(currentModeIndex);
                if (mode) {
                    ImGui::SameLine();
                    ImGui::Text("(%s)", mode->modeName.c_str());
                }
            }
        }
        
        // Apply changes
        if (changed) {
            cellManager.updateCellData(selectedCell.cellIndex, editedCell);
        }
        
        ImGui::Separator();
        
        // Action buttons
        if (ImGui::Button("Clear Selection")) {
            cellManager.clearSelection();
        }
          // Status
        if (cellManager.isDraggingCell) {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "DRAGGING");
            ImGui::Text("Drag Distance: %.2f", selectedCell.dragDistance);
            ImGui::Text("(Use scroll wheel to adjust distance)");
        }    } else {
        ImGui::Text("No cell selected");
        ImGui::Separator();
        ImGui::Text("Instructions:");
        ImGui::BulletText("Left-click on a cell to select it");
        ImGui::BulletText("Drag to move selected cell");
        ImGui::BulletText("Scroll wheel to adjust distance");
        ImGui::BulletText("Selected cell moves in a plane");
        ImGui::BulletText("parallel to the camera");
    }
    
    ImGui::End();
}

void UIManager::renderSelectionInfo(CellManager& cellManager) {
    // This could be used for a minimal selection display overlay
    if (cellManager.hasSelectedCell()) {
        const auto& selectedCell = cellManager.getSelectedCell();
        
        // Create a small overlay window
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                               ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
        
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::Begin("Selection Overlay", nullptr, flags);
        
        ImGui::Text("Cell #%d selected", selectedCell.cellIndex);
        if (cellManager.isDraggingCell) {
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Dragging...");
        }
        
        ImGui::End();
    }
}

void UIManager::renderSimulationControls(CellManager& cellManager) {
    ImGui::Begin("Simulation Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    // Current simulation status
    ImGui::Text("Cells: %d", cellManager.getCellCount());
    
    // Reset simulation section
    ImGui::Separator();
    ImGui::Text("Simulation Control:");
    
    // Reset with default count (1 cell)
    if (ImGui::Button("Reset Simulation")) {
        cellManager.resetSimulation(1);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted("Clears all cells and starts fresh with 1 cell at the origin.");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    
    // Reset with custom count
    static int resetCellCount = 5;
    ImGui::SliderInt("Reset Count", &resetCellCount, 1, 20);
    
    if (ImGui::Button("Reset with Custom Count")) {
        cellManager.resetSimulation(resetCellCount);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted("Clears all cells and spawns the specified number of cells randomly distributed.");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    
    ImGui::End();
}

void UIManager::drawToolSelector(ToolState& toolState) {
    const char* tools[] = { "None", "Add", "Edit", "Move (UNIMPLEMENTED)" };
    int current = static_cast<int>(toolState.activeTool);
    if (ImGui::Combo("Tool", &current, tools, IM_ARRAYSIZE(tools))) {
        toolState.activeTool = static_cast<ToolType>(current);
    }
}

void UIManager::drawToolSettings(ToolState& toolState, CellManager& cellManager) {
    switch (toolState.activeTool) {
    case ToolType::AddCell:
        ImGui::ColorEdit4("New Cell Color", &toolState.newCellColor[0]);
        ImGui::SliderFloat("New Cell Mass", &toolState.newCellMass,0.1f, 10.0f, "%.2f", ImGuiSliderFlags_Logarithmic);
        break;
    case ToolType::EditCell:
        
    default:
        break;
    }
}

void UIManager::renderPerformanceMonitor(CellManager& cellManager, PerformanceMonitor& perfMonitor)
{
    ImGui::Begin("Performance Monitor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("FPS: %.1f", perfMonitor.displayFPS);
    ImGui::Text("Frame Time: %.3f ms", perfMonitor.displayFrameTime);
    ImGui::Text("Cells: %d", cellManager.getCellCount());

    // Visual performance indicators
    float targetFPS = 60.0f;
    ImGui::Text("Performance:");
    ImGui::SameLine();
    if (perfMonitor.displayFPS >= targetFPS) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "GOOD");
    }
    else if (perfMonitor.displayFPS >= 30.0f) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "OK");
    }
    else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "POOR");
    }

    // Technical details
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    if (renderer) ImGui::Text("GPU: %s", renderer);
    ImGui::Text("Total triangles: ~%d", 192 * cellManager.getCellCount());
    ImGui::End();
}

void UIManager::renderCameraControls(CellManager& cellManager, Camera& camera)
{
    ImGui::Begin("Camera & Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    glm::vec3 camPos = camera.getPosition();
    ImGui::Text("Position: (%.2f, %.2f, %.2f)", camPos.x, camPos.y, camPos.z);
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
    if (cellManager.hasSelectedCell()) {
        ImGui::Separator();
        const auto& selection = cellManager.getSelectedCell();
        ImGui::Text("Selected: Cell #%d", selection.cellIndex);
        ImGui::Text("Drag Distance: %.1f", selection.dragDistance);
    }
    ImGui::End();
}

void UIManager::renderGenomeEditor() {
    if (!g_genomeSystem) return;
    
    ImGui::Begin("Genome Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    // Header info with helpful context
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 1.0f, 1.0f), "Single-Cell Evolution System");
    ImGui::Separator();
      // Header info
    ImGui::Text("Genome Modes: %zu", g_genomeSystem->getModeCount());
    
    // Show which mode is the initial mode
    int initialModeIndex = g_genomeSystem->getInitialModeIndex();
    if (initialModeIndex >= 0) {
        const GenomeMode* initialMode = g_genomeSystem->getMode(initialModeIndex);
        if (initialMode) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "| Initial: %s", initialMode->modeName.c_str());
        }
    }
      // Add/Remove modes
    if (ImGui::Button("Add New Mode")) {
        GenomeMode newMode;
        newMode.modeName = "New Mode " + std::to_string(g_genomeSystem->getModeCount());
        newMode.modeColor = GenomeUtils::randomColor();
        newMode.isInitial = false; // New modes are never initial by default
        g_genomeSystem->addMode(newMode);
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Load Defaults")) {
        g_genomeSystem->loadDefaultModes();
    }
    
    ImGui::Separator();
    
    // Display each mode
    for (size_t i = 0; i < g_genomeSystem->getModeCount(); ++i) {
        drawGenomeModeEditor(static_cast<int>(i));
    }
    
    ImGui::End();
}

void UIManager::drawGenomeModeEditor(int modeIndex) {
    if (!g_genomeSystem) return;
    
    GenomeMode* mode = g_genomeSystem->getMode(modeIndex);
    if (!mode) return;
    
    // Create collapsible header for each mode
    ImGui::PushID(modeIndex);
    
    // Color the header based on the mode color
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(mode->modeColor.r * 0.5f, mode->modeColor.g * 0.5f, mode->modeColor.b * 0.5f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(mode->modeColor.r * 0.7f, mode->modeColor.g * 0.7f, mode->modeColor.b * 0.7f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(mode->modeColor.r * 0.9f, mode->modeColor.g * 0.9f, mode->modeColor.b * 0.9f, 1.0f));
    
    bool isOpen = ImGui::CollapsingHeader(mode->modeName.c_str());
    
    ImGui::PopStyleColor(3);
    
    if (isOpen) {
        GenomeMode editedMode = *mode;
        bool changed = false;        // Basic properties
        char nameBuffer[128];
        size_t copyLen = std::min(editedMode.modeName.length(), sizeof(nameBuffer) - 1);
        std::memcpy(nameBuffer, editedMode.modeName.c_str(), copyLen);
        nameBuffer[copyLen] = '\0'; // Ensure null termination
        if (ImGui::InputText("Mode Name", nameBuffer, sizeof(nameBuffer))) {
            editedMode.modeName = nameBuffer;
            changed = true;
        }
          // Initial mode checkbox - enforce only one initial mode
        bool wasInitial = editedMode.isInitial;
        if (ImGui::Checkbox("Initial Mode", &editedMode.isInitial)) {
            // If this mode is being set to initial, clear all other initial flags
            if (editedMode.isInitial && !wasInitial) {
                // Clear initial flag from all other modes
                for (size_t i = 0; i < g_genomeSystem->getModeCount(); ++i) {
                    if (i != modeIndex) {
                        GenomeMode* otherMode = g_genomeSystem->getMode(static_cast<int>(i));
                        if (otherMode && otherMode->isInitial) {
                            GenomeMode updatedOtherMode = *otherMode;
                            updatedOtherMode.isInitial = false;
                            g_genomeSystem->updateMode(static_cast<int>(i), updatedOtherMode);
                        }
                    }
                }
            }
            // Don't allow unchecking if this is the only mode
            else if (!editedMode.isInitial && wasInitial && g_genomeSystem->getModeCount() == 1) {
                editedMode.isInitial = true; // Force it back to true
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(Must have one initial mode)");
            }
            changed = true;
        }
        
        // Color picker
        float color[4] = { editedMode.modeColor.r, editedMode.modeColor.g, editedMode.modeColor.b, editedMode.modeColor.a };
        if (ImGui::ColorEdit4("Mode Color", color)) {
            editedMode.modeColor = Color(color[0], color[1], color[2], color[3]);
            changed = true;
        }
          // Split interval
        if (ImGui::SliderFloat("Split Interval", &editedMode.splitInterval, 1.0f, 15.0f, "%.1f sec")) {
            changed = true;
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80);        if (ImGui::DragFloat("##SplitIntervalInput", &editedMode.splitInterval, 0.1f, 1.0f, 15.0f, "%.1f")) {
            editedMode.splitInterval = clamp(editedMode.splitInterval, 1.0f, 15.0f);
            changed = true;
        }
        
        // Parent split settings
        if (ImGui::TreeNode("Parent Split Settings")) {
            if (ImGui::Checkbox("Parent Make Adhesion", &editedMode.parentMakeAdhesion)) {
                changed = true;
            }
              if (ImGui::SliderFloat("Split Yaw", &editedMode.parentSplitYaw, -180.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##SplitYawInput", &editedMode.parentSplitYaw, 1.0f, -180.0f, 180.0f, "%.1f")) {
                editedMode.parentSplitYaw = clamp(editedMode.parentSplitYaw, -180.0f, 180.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Split Pitch", &editedMode.parentSplitPitch, -90.0f, 90.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##SplitPitchInput", &editedMode.parentSplitPitch, 1.0f, -90.0f, 90.0f, "%.1f")) {
                editedMode.parentSplitPitch = clamp(editedMode.parentSplitPitch, -90.0f, 90.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Split Roll", &editedMode.parentSplitRoll, -180.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##SplitRollInput", &editedMode.parentSplitRoll, 1.0f, -180.0f, 180.0f, "%.1f")) {
                editedMode.parentSplitRoll = clamp(editedMode.parentSplitRoll, -180.0f, 180.0f);
                changed = true;
            }
            
            ImGui::TreePop();
        }
        
        // Child A settings
        if (ImGui::TreeNode("Child A Settings")) {
            int maxModes = static_cast<int>(g_genomeSystem->getModeCount());
              if (ImGui::SliderInt("Mode Index", &editedMode.childAModeIndex, -1, maxModes - 1)) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(-1 = use parent mode)");
            
            if (ImGui::SliderFloat("Orientation Yaw", &editedMode.childA_OrientationYaw, -180.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##ChildAYawInput", &editedMode.childA_OrientationYaw, 1.0f, -180.0f, 180.0f, "%.1f")) {
                editedMode.childA_OrientationYaw = clamp(editedMode.childA_OrientationYaw, -180.0f, 180.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Orientation Pitch", &editedMode.childA_OrientationPitch, -90.0f, 90.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##ChildAPitchInput", &editedMode.childA_OrientationPitch, 1.0f, -90.0f, 90.0f, "%.1f")) {
                editedMode.childA_OrientationPitch = clamp(editedMode.childA_OrientationPitch, -90.0f, 90.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Orientation Roll", &editedMode.childA_OrientationRoll, -180.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##ChildARollInput", &editedMode.childA_OrientationRoll, 1.0f, -180.0f, 180.0f, "%.1f")) {
                editedMode.childA_OrientationRoll = clamp(editedMode.childA_OrientationRoll, -180.0f, 180.0f);
                changed = true;
            }
            
            if (ImGui::Checkbox("Keep Adhesion", &editedMode.childA_KeepAdhesion)) {
                changed = true;
            }
            
            ImGui::TreePop();
        }
        
        // Child B settings
        if (ImGui::TreeNode("Child B Settings")) {
            int maxModes = static_cast<int>(g_genomeSystem->getModeCount());
            
            if (ImGui::SliderInt("Mode Index", &editedMode.childBModeIndex, -1, maxModes - 1)) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(-1 = use parent mode)");
              if (ImGui::SliderFloat("Orientation Yaw", &editedMode.childB_OrientationYaw, -180.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##ChildBYawInput", &editedMode.childB_OrientationYaw, 1.0f, -180.0f, 180.0f, "%.1f")) {
                editedMode.childB_OrientationYaw = clamp(editedMode.childB_OrientationYaw, -180.0f, 180.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Orientation Pitch", &editedMode.childB_OrientationPitch, -90.0f, 90.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##ChildBPitchInput", &editedMode.childB_OrientationPitch, 1.0f, -90.0f, 90.0f, "%.1f")) {
                editedMode.childB_OrientationPitch = clamp(editedMode.childB_OrientationPitch, -90.0f, 90.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Orientation Roll", &editedMode.childB_OrientationRoll, -180.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##ChildBRollInput", &editedMode.childB_OrientationRoll, 1.0f, -180.0f, 180.0f, "%.1f")) {
                editedMode.childB_OrientationRoll = clamp(editedMode.childB_OrientationRoll, -180.0f, 180.0f);
                changed = true;
            }
            
            if (ImGui::Checkbox("Keep Adhesion", &editedMode.childB_KeepAdhesion)) {
                changed = true;
            }
            
            ImGui::TreePop();
        }
        
        // Adhesion settings
        if (ImGui::TreeNode("Adhesion Settings")) {            if (ImGui::SliderFloat("Rest Length", &editedMode.adhesionRestLength, 1.0f, 10.0f, "%.1f")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##RestLengthInput", &editedMode.adhesionRestLength, 0.1f, 1.0f, 10.0f, "%.1f")) {
                editedMode.adhesionRestLength = clamp(editedMode.adhesionRestLength, 1.0f, 10.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Spring Stiffness", &editedMode.adhesionSpringStiffness, 10.0f, 500.0f, "%.1f")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##SpringStiffnessInput", &editedMode.adhesionSpringStiffness, 5.0f, 10.0f, 500.0f, "%.1f")) {
                editedMode.adhesionSpringStiffness = clamp(editedMode.adhesionSpringStiffness, 10.0f, 500.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Spring Damping", &editedMode.adhesionSpringDamping, 0.0f, 100.0f, "%.1f")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            if (ImGui::DragFloat("##SpringDampingInput", &editedMode.adhesionSpringDamping, 1.0f, 0.0f, 100.0f, "%.1f")) {
                editedMode.adhesionSpringDamping = clamp(editedMode.adhesionSpringDamping, 0.0f, 100.0f);
                changed = true;
            }
            
            if (ImGui::Checkbox("Can Break", &editedMode.adhesionCanBreak)) {
                changed = true;
            }
              if (editedMode.adhesionCanBreak) {                if (ImGui::SliderFloat("Break Force", &editedMode.adhesionBreakForce, 100.0f, 5000.0f, "%.1f")) {
                    changed = true;
                }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                if (ImGui::DragFloat("##BreakForceInput", &editedMode.adhesionBreakForce, 50.0f, 100.0f, 5000.0f, "%.1f")) {
                    editedMode.adhesionBreakForce = clamp(editedMode.adhesionBreakForce, 100.0f, 5000.0f);
                    changed = true;
                }
            }
            
            ImGui::TreePop();
        }
        
        // Orientation constraints
        if (ImGui::TreeNode("Orientation Constraints")) {            if (ImGui::SliderFloat("Constraint Strength", &editedMode.orientationConstraintStrength, 0.0f, 1.0f, "%.2f")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##ConstraintStrengthInput", &editedMode.orientationConstraintStrength, 0.01f, 0.0f, 1.0f, "%.2f")) {
                editedMode.orientationConstraintStrength = clamp(editedMode.orientationConstraintStrength, 0.0f, 1.0f);
                changed = true;
            }
            
            if (ImGui::SliderFloat("Max Angle Deviation", &editedMode.maxAllowedAngleDeviation, 0.0f, 180.0f, "%.1f°")) {
                changed = true;
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);            if (ImGui::DragFloat("##MaxAngleDeviationInput", &editedMode.maxAllowedAngleDeviation, 1.0f, 0.0f, 180.0f, "%.1f")) {
                editedMode.maxAllowedAngleDeviation = clamp(editedMode.maxAllowedAngleDeviation, 0.0f, 180.0f);
                changed = true;
            }
            
            ImGui::TreePop();
        }
          // Remove mode button
        ImGui::Separator();
        if (g_genomeSystem->getModeCount() > 1) {
            // Check if this is the initial mode
            bool isInitialMode = editedMode.isInitial;
            if (isInitialMode) {
                // If this is the initial mode, warn the user
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
                if (ImGui::Button("Remove Mode")) {
                    // Do nothing - can't remove initial mode
                }
                ImGui::PopStyleColor(3);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "(Cannot remove initial mode)");
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button("Remove Mode")) {
                    g_genomeSystem->removeMode(modeIndex);
                    ImGui::PopStyleColor();
                    ImGui::PopID();
                    return; // Exit early since mode was deleted
                }
                ImGui::PopStyleColor();
            }
        } else {
            ImGui::TextDisabled("(Cannot remove the last mode)");
        }
        
        // Apply changes
        if (changed) {
            g_genomeSystem->updateMode(modeIndex, editedMode);
        }
    }
    
    ImGui::PopID();
}