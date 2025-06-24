#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "camera.h"
#include "genome.h"

// Forward declarations
struct CellManager; // Forward declaration to avoid circular dependency
class SceneManager; // Forward declaration for scene management
struct ComputeCell; // Forward declaration for keyframe system

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
    void renderCellInspector(CellManager &cellManager, SceneManager& sceneManager);
    void renderPerformanceMonitor(CellManager &cellManager, PerformanceMonitor &perfMonitor, SceneManager& sceneManager);
    void renderCameraControls(CellManager &cellmanager, Camera &camera, SceneManager& sceneManager);
    void renderGenomeEditor(CellManager& cellManager, SceneManager& sceneManager);
    void renderTimeScrubber(CellManager& cellManager, SceneManager& sceneManager); // New time scrubber window
    void renderSceneSwitcher(SceneManager& sceneManager, CellManager& previewCellManager, CellManager& mainCellManager); // Scene switcher window

    // Preview simulation time control
    void updatePreviewSimulation(CellManager& previewCellManager);

    // Performance monitoring helpers
    void updatePerformanceMetrics(PerformanceMonitor &perfMonitor, float deltaTime);
    GenomeData currentGenome;

    // Scene management
    void switchToScene(int sceneIndex); // Method to switch scenes

    void checkKeyframeTimingAccuracy();

    // Genome change tracking
    bool genomeChanged = false;          // Flag to indicate genome was modified
    
    // Orientation gizmo visualization
    bool showOrientationGizmos = false;  // Toggle for showing cell orientation gizmos

private:    // Helper to get window flags based on lock state
    int getWindowFlags(int baseFlags = 0) const;
    
    void drawToolSelector(ToolState &toolState);
    void drawToolSettings(ToolState &toolState, CellManager &cellManager); // Genome Editor Helper Functions
    void drawModeSelector(GenomeData &genome);
    void drawModeSettings(ModeSettings &mode, int modeIndex, CellManager& cellManager);
    void drawParentSettings(ModeSettings &mode);
    void drawChildSettings(const char *label, ChildSettings &child);
    void drawAdhesionSettings(AdhesionSettings &adhesion);
    void drawSliderWithInput(const char *label, float *value, float min, float max, const char *format = "%.2f", float step = 0.0f);
    void drawColorPicker(const char *label, glm::vec3 *color);
    void addTooltip(const char* tooltip); // Helper to add question mark tooltips
    bool isColorBright(const glm::vec3 &color); // Helper to determine if color is bright    // Genome Editor Data
    int selectedModeIndex = 0;    // Time Scrubber Data
    float currentTime = 0.0f;
    float maxTime = 50.0f;
    char timeInputBuffer[32] = "0.00";
    float simulatedTime = 0.0f;  // Actual simulated time in preview
    float targetTime = 0.0f;     // Target time we want to scrub to
    bool needsSimulationReset = false;  // Flag to reset simulation when scrubber changes
    bool isScrubbingTime = false;       // Flag to indicate we're scrubbing to a specific time
    
    // Keyframe system for efficient time scrubbing
    struct SimulationKeyframe {
        float time = 0.0f;
        std::vector<ComputeCell> cellStates;
        GenomeData genome;
        int cellCount = 0;
        bool isValid = false;
    };
    
    static constexpr int MAX_KEYFRAMES = 50;
    std::vector<SimulationKeyframe> keyframes;
    bool keyframesInitialized = false;
    
    void initializeKeyframes(CellManager& cellManager);
    void updateKeyframes(CellManager& cellManager, float newMaxTime);
    int findNearestKeyframe(float targetTime) const;
    void restoreFromKeyframe(CellManager& cellManager, int keyframeIndex);
    void captureKeyframe(CellManager& cellManager, float time, int keyframeIndex);
    
    // Window management
    bool windowsLocked = true;

    void applyLocalRotation(glm::quat& orientation, const glm::vec3& axis, float delta);
};