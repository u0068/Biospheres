#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "camera.h"
#include "genome.h"

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
    glm::vec4 newCellColor = {1.0, 1.0, 1.0, 1.0};
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

    // Advanced metrics
    float minFrameTime = 1000.0f;
    float maxFrameTime = 0.0f;
    float avgFrameTime = 0.0f;
    std::vector<float> frameTimeHistory;
    std::vector<float> fpsHistory;
    static constexpr int HISTORY_SIZE = 120; // 2 seconds at 60fps

    // GPU metrics
    float gpuMemoryUsed = 0.0f;
    float gpuMemoryTotal = 0.0f;
    int drawCalls = 0;
    int vertices = 0;

    // CPU metrics
    float cpuUsage = 0.0f;
    float memoryUsage = 0.0f;

    // Timing breakdown
    float updateTime = 0.0f;
    float renderTime = 0.0f;
    float uiTime = 0.0f;
};


class UIManager
{
public:
    void renderCellInspector(CellManager &cellManager);
    void renderPerformanceMonitor(CellManager &cellManager, PerformanceMonitor &perfMonitor);
    void renderCameraControls(CellManager &cellmanager, Camera &camera);
    void renderGenomeEditor();
    void renderTimeScrubber(CellManager& cellManager); // New time scrubber window

    // Performance monitoring helpers
    void updatePerformanceMetrics(PerformanceMonitor &perfMonitor, float deltaTime);
    GenomeData currentGenome;

private:
    void drawToolSelector(ToolState &toolState);
    void drawToolSettings(ToolState &toolState, CellManager &cellManager); // Genome Editor Helper Functions
    void drawModeSelector(GenomeData &genome);
    void drawModeSettings(ModeSettings &mode, int modeIndex);
    void drawParentSettings(ModeSettings &mode);
    void drawChildSettings(const char *label, ChildSettings &child);
    void drawAdhesionSettings(AdhesionSettings &adhesion);
    void drawSliderWithInput(const char *label, float *value, float min, float max, const char *format = "%.2f", float step = 0.0f);
    void drawColorPicker(const char *label, glm::vec3 *color);
    bool isColorBright(const glm::vec3 &color); // Helper to determine if color is bright    // Genome Editor Data
    int selectedModeIndex = 0;
    
    // Time Scrubber Data
    float currentTime = 0.0f;
    float maxTime = 100.0f;
    char timeInputBuffer[32] = "0.00";
};