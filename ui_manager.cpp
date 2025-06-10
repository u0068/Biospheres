#include "ui_manager.h"
#include "cell_manager.h"
#include "imgui.h"

// All of this is outdated and needs to be redone for 3D

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
