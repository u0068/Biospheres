#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"
#include "../../scene/scene_manager.h"

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

class PerformancePanel
{
public:
    void render(CellManager &cellManager, PerformanceMonitor &perfMonitor, SceneManager& sceneManager);
    void updatePerformanceMetrics(PerformanceMonitor &perfMonitor, float deltaTime);

private:
    bool windowsLocked = true;
    int getWindowFlags(int baseFlags = 0) const;
}; 