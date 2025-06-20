#pragma once

enum class Scene
{
    PreviewSimulation,
    MainSimulation,
    // Add more scenes as needed
};

class SceneManager
{
public:
    SceneManager() : currentScene(Scene::PreviewSimulation), sceneChanged(false), paused(true), simulationSpeed(1.0f), previewPaused(true), mainPaused(false) {}
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
    }    void resetSpeed() { simulationSpeed = 1.0f; }
    
    // Preview simulation time tracking
    float getPreviewSimulationTime() const { return previewSimulationTime; }
    void setPreviewSimulationTime(float time) { previewSimulationTime = time; }
    void updatePreviewSimulationTime(float deltaTime) 
    { 
        if (!paused && currentScene == Scene::PreviewSimulation) 
        {
            previewSimulationTime += deltaTime * simulationSpeed;
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

private:
    Scene currentScene;
    bool sceneChanged = false;
    bool paused = false;
    float simulationSpeed = 1.0f;
    float previewSimulationTime = 0.0f;  // Track time in preview simulation
    
    // Per-scene pause states
    bool previewPaused = true;   // Preview starts paused
    bool mainPaused = false;     // Main starts unpaused
};
