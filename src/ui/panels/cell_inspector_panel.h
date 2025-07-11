#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"
#include "../../scene/scene_manager.h"

class CellInspectorPanel
{
public:
    void render(CellManager &cellManager, SceneManager& sceneManager);

private:
    bool windowsLocked = true;
    int getWindowFlags(int baseFlags = 0) const;
}; 