#include "ui_manager.h"
#include "cell_manager.h"
#include "imgui.h"

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