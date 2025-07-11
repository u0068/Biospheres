#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"
#include "../../scene/scene_manager.h"

class SceneSwitcherPanel
{
public:
    void render(SceneManager& sceneManager, CellManager& previewCellManager, CellManager& mainCellManager);
    void switchToScene(int sceneIndex); // Method to switch scenes

private:
    bool windowsLocked = true;
    int getWindowFlags(int baseFlags = 0) const;
}; 