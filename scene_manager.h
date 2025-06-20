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
    SceneManager() : currentScene(Scene::PreviewSimulation), sceneChanged(false), paused(false), simulationSpeed(1.0f) {}
    
    Scene getCurrentScene() const { return currentScene; }
    void switchToScene(Scene newScene) 
    { 
        if (currentScene != newScene)
        {
            currentScene = newScene;
            sceneChanged = true;
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
        if (speed > 5.0f) speed = 5.0f;
        simulationSpeed = speed;
    }
	void resetSpeed() { simulationSpeed = 1.0f; }

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
};
