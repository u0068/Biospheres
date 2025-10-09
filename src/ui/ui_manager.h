#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../rendering/camera/camera.h"
#include "../simulation/cell/common_structs.h"

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
    void renderSimulationSettings(CellManager& cellManager); // Simulation settings window (voxel grid, etc.)

    // Preview simulation time control
    void updatePreviewSimulation(CellManager& previewCellManager);
    
    // Debounced genome resimulation handler
    void updateDebouncedGenomeResimulation(CellManager& cellManager, SceneManager& sceneManager, float deltaTime);

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
    
    // Adhesion line visualization
    bool showAdhesionLines = true;      // Toggle for showing adhesionSettings lines between sibling cells
    
    // Wireframe rendering mode
    bool wireframeMode = false;          // Toggle for wireframe rendering mode
    
    // Frustum culling toggle
    bool enableFrustumCulling = true;    // Toggle for frustum culling
    
    // Distance-based culling and fading toggle
    bool enableDistanceCulling = true;   // Toggle for distance-based culling and fading
    
    // Voxel grid visualization toggles
    bool showVoxelGrid = false;          // Toggle for showing voxel grid lines
    bool showVoxelCubes = false;         // Toggle for showing nutrient voxel cubes

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
    void drawSliderWithInput(const char* label, int* value, int min, int max, int step = 1);
    void drawColorPicker(const char *label, glm::vec3 *color);
    
    // Flagellocyte settings helpers
    void saveFlagellocyteSettings(const FlagellocyteSettings& settings, const std::string& modeName);
    bool loadFlagellocyteSettings(FlagellocyteSettings& settings, const std::string& modeName);
    glm::vec3 normalizeColor(const glm::vec3& color); // Helper to normalize color values
    void validateGenomeColors(); // Validate and fix color values in genome
    void addTooltip(const char* tooltip); // Helper to add question mark tooltips
    bool isColorBright(const glm::vec3 &color); // Helper to determine if color is bright    // Genome Editor Data
    int selectedModeIndex = 0;
    
    // Debouncing for genome changes with immediate update on mouse release
    float genomeChangeDebounceTimer = 0.0f;
    static constexpr float GENOME_CHANGE_DEBOUNCE_DELAY = 0.3f; // 300ms delay if mouse not released
    static constexpr float GENOME_PERIODIC_UPDATE_INTERVAL = 0.05f; // Update genome buffer every 50ms during slider drag
    float periodicUpdateTimer = 0.0f;
    bool pendingGenomeResimulation = false;
    bool isResimulating = false;  // Flag to indicate resimulation in progress
    float resimulationProgress = 0.0f;  // Progress of resimulation (0.0 to 1.0)
    bool wasMouseDownLastFrame = false; // Track mouse state to detect release    // Time Scrubber Data
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
        std::vector<AdhesionConnection> adhesionConnections; // Store adhesion connections
        GenomeData genome;
        int cellCount = 0;
        int adhesionCount = 0; // Store adhesion count
        bool isValid = false;
    };
    
    static constexpr int MAX_KEYFRAMES = 400;
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