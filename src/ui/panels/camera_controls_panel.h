#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../scene/scene_manager.h"

class CameraControlsPanel
{
public:
    void render(CellManager &cellManager, Camera &camera, SceneManager& sceneManager);

private:
    bool windowsLocked = true;
    int getWindowFlags(int baseFlags = 0) const;
}; 