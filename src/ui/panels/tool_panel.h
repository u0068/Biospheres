#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"

enum class ToolType : std::uint8_t
{
    None,
    AddCell,
    EditCell,
    MoveCell,
    // ... more tools as needed
};

struct ToolState
{
    ToolType activeTool = ToolType::None;
    int selectedCellIndex = -1; // For selection/editing
    // Settings per tool
    glm::vec4 newCellColor = {1.0, 1.0, 1.0, 1.0};
    float newCellMass = 1.0f;
};

class ToolPanel
{
public:
    void render(ToolState &toolState, CellManager &cellManager);

private:
    void drawToolSelector(ToolState &toolState);
    void drawToolSettings(ToolState &toolState, CellManager &cellManager);
}; 