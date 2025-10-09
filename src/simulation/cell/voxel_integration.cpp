#include "cell_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../utils/timer.h"

// Voxel nutrient system integration
// 16³ voxel grid for nutrients (much coarser than 64³ spatial grid for performance)
// Cells use spatial grid for neighbors, voxel grid for nutrients

void CellManager::initializeVoxelSystem()
{
    TimerCPU timer("Voxel System Initialization");
    voxelManager.initialize();
}

void CellManager::updateVoxelSystem(float deltaTime)
{
    TimerCPU timer("Voxel System Update");
    voxelManager.update(deltaTime);
}

void CellManager::renderVoxelSystem(const Camera& camera, const glm::vec2& resolution, bool showGridLines, bool showVoxels)
{
    TimerCPU timer("Voxel System Render");
    voxelManager.renderVoxelGrid(camera, resolution, showGridLines, showVoxels, 
                                 maxRenderDistance, fadeStartDistance);
}

void CellManager::cleanupVoxelSystem()
{
    voxelManager.cleanup();
}
