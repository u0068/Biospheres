#pragma once
#include <map>
#include <string>
#include "../core/config.h"

enum class Scene
{
    PreviewSimulation,
    MainSimulation,
    // Add more scenes as needed
};

class SceneManager
{
public:
    SceneManager() : currentScene(Scene::PreviewSimulation), sceneChanged(false), paused(true), simulationSpeed(1.0f), previewPaused(true), mainPaused(false) {
        sceneCellLimits[Scene::PreviewSimulation] = 256;
        sceneCellLimits[Scene::MainSimulation] = config::MAX_CELLS;
    }
      Scene getCurrentScene() const { return currentScene; }
    void switchToScene(Scene newScene) 
    { 
        if (currentScene != newScene)
        {
            // Store pause state for current scene
            if (currentScene == Scene::PreviewSimulation)
            {
                previewPaused = paused;
            }
            else if (currentScene == Scene::MainSimulation)
            {
                mainPaused = paused;
            }
            
            currentScene = newScene;
            sceneChanged = true;
            
            // Restore pause state for new scene
            if (currentScene == Scene::PreviewSimulation)
            {
                paused = previewPaused;
            }
            else if (currentScene == Scene::MainSimulation)
            {
                paused = mainPaused;
            }
        }
    }
    
    bool hasSceneChanged() 
    { 
        bool changed = sceneChanged;
        sceneChanged = false;
        return changed;
    }
      // Pause/unpause functionality
    bool isPaused() const { return paused; }
    void setPaused(bool pauseState) { paused = pauseState; }
    void togglePause() { paused = !paused; }
      // Speed control functionality
    float getSimulationSpeed() const { return simulationSpeed; }
    void setSimulationSpeed(float speed) 
    { 
        // Clamp between 0.1x and 5.0x manually to avoid Windows macro conflicts
        if (speed < 0.1f) speed = 0.1f;
        if (speed > 10.0f) speed = 10.0f;
        simulationSpeed = speed;
    }
	void resetSpeed() { simulationSpeed = 1.0f; }
    
    // Preview simulation time tracking
    float getPreviewSimulationTime() const { return previewSimulationTime; }
    void setPreviewSimulationTime(float time) { previewSimulationTime = time; }
    void updatePreviewSimulationTime(float deltaTime) 
    { 
        if (!paused && currentScene == Scene::PreviewSimulation) 
        {
            previewSimulationTime += deltaTime;// *simulationSpeed;
        }
    }
    void resetPreviewSimulationTime() { previewSimulationTime = 0.0f; }

    const char* getSceneName(Scene scene) const
    {
        switch (scene)
        {
            case Scene::PreviewSimulation: return "Preview Simulation";
            case Scene::MainSimulation: return "Main Simulation";
            default: return "Unknown";
        }
    }
    
    const char* getCurrentSceneName() const
    {
        return getSceneName(currentScene);
    }

    // Per-scene cell limits
    void setCellLimit(Scene scene, int limit) { sceneCellLimits[scene] = limit; }
    int getCellLimit(Scene scene) const {
        auto it = sceneCellLimits.find(scene);
        if (it != sceneCellLimits.end()) return it->second;
        return config::MAX_CELLS;
    }
    int getCurrentCellLimit() const { return getCellLimit(currentScene); }

    // Scene file management (Requirements 3.4, 3.5)
    void loadPreviewScene(const std::string& filename);
    void savePreviewScene(const std::string& filename);
    void loadMainScene(const std::string& filename);
    void saveMainScene(const std::string& filename);
    
    // Scene switching without data conversion (Requirement 3.4)
    void switchToPreviewMode();
    void switchToMainMode();
    bool isPreviewMode() const { return currentScene == Scene::PreviewSimulation; }
    bool isMainMode() const { return currentScene == Scene::MainSimulation; }
    
    // Independent system coordination
    void setPreviewSystemActive(bool active) { previewSystemActive = active; }
    void setMainSystemActive(bool active) { mainSystemActive = active; }
    bool isPreviewSystemActive() const { return previewSystemActive; }
    bool isMainSystemActive() const { return mainSystemActive; }
    
    // System coordination interface (Requirements 3.4, 3.5)
    void coordinateWithCPUPreviewSystem(class CPUPreviewSystem* cpuSystem) { m_cpuPreviewSystem = cpuSystem; }
    void coordinateWithMainCellManager(class CellManager* cellManager) { m_mainCellManager = cellManager; }
    
    // File handling coordination
    std::string getCurrentPreviewSceneFile() const { return currentPreviewSceneFile; }
    std::string getCurrentMainSceneFile() const { return currentMainSceneFile; }

private:
    Scene currentScene;
    bool sceneChanged = false;
    bool paused = false;
    float simulationSpeed = 1.0f;
    float previewSimulationTime = 0.0f;  // Track time in preview simulation
    
    // Per-scene pause states
    bool previewPaused = true;   // Preview starts paused
    bool mainPaused = false;     // Main starts unpaused

    std::map<Scene, int> sceneCellLimits{{Scene::PreviewSimulation, config::MAX_CELLS}, {Scene::MainSimulation, config::MAX_CELLS}};
    
    // Independent system states (Requirements 3.4, 3.5)
    bool previewSystemActive = false;
    bool mainSystemActive = false;
    std::string currentPreviewSceneFile;
    std::string currentMainSceneFile;
    
    // System coordination pointers
    class CPUPreviewSystem* m_cpuPreviewSystem = nullptr;
    class CellManager* m_mainCellManager = nullptr;
};
