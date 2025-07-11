#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"
#include "../../simulation/cell/common_structs.h"
#include "../../scene/scene_manager.h"

class TimeScrubberPanel
{
public:
    void render(CellManager& cellManager, SceneManager& sceneManager);
    void updatePreviewSimulation(CellManager& previewCellManager);
    void checkKeyframeTimingAccuracy();

private:
    bool windowsLocked = true;
    int getWindowFlags(int baseFlags = 0) const;

    // Time Scrubber Data
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
}; 