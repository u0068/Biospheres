#pragma once
#include <cstdint>
#include <glm.hpp>

struct CellManager; // Forward declaration to avoid circular dependency

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
    glm::vec4 newCellColor = { 1.0, 1.0, 1.0, 1.0 };
    float newCellMass = 1.0f;
};

class UIManager
{
public:
private:
    void drawToolSelector(ToolState& toolState);
    void drawToolSettings(ToolState& toolState, CellManager& cellManager);
};
