#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include "camera.h"

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

struct PerformanceMonitor
{
    float lastPerfUpdate = 0.0f;
    float perfUpdateInterval = 0.25f; // Update every 250ms
    float displayFPS = 0.0f;
    float displayFrameTime = 0.0f;
    int frameCount = 0;
    float frameTimeAccumulator = 0.0f;
};

class UIManager
{
public:
    void renderCellInspector(CellManager& cellManager);
    void renderSelectionInfo(CellManager& cellManager);
    void renderPerformanceMonitor(CellManager& cellManager, PerformanceMonitor& perfMonitor);
    void renderCameraControls(CellManager& cellmanager, Camera& camera);
    void renderGenomeEditor(CellManager& cellManager);

    
private:
    void drawToolSelector(ToolState& toolState);
    void drawToolSettings(ToolState& toolState, CellManager& cellManager);
};