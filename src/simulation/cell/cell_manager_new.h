#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../rendering/core/shader_class.h"
#include "../cell/common_structs.h"

// Component managers
#include "buffer_manager.h"
#include "physics_manager.h"
#include "spatial_manager.h"
#include "rendering_manager.h"
#include "interaction_manager.h"
#include "gizmo_manager.h"
#include "barrier_manager.h"

// Forward declaration
class Camera;

struct CellManager
{
    // Component managers
    CellBufferManager bufferManager;
    CellPhysicsManager physicsManager;
    CellSpatialManager spatialManager;
    CellRenderingManager renderingManager;
    CellInteractionManager interactionManager;
    CellGizmoManager gizmoManager;
    CellBarrierManager barrierManager;

    // Constructor and destructor
    CellManager();
    ~CellManager();

    // Main update and render functions
    void updateCells(float deltaTime);
    void renderCells(glm::vec2 resolution, Shader &cellShader, Camera &camera, bool wireframe = false);
    void cleanup();

    // Cell management
    void spawnCells(int count = 1000);
    void addCell(const ComputeCell &newCell);
    void resetSimulation();

    // Interaction
    void handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                          const Camera &camera, bool isMousePressed, bool isMouseDown,
                          float scrollDelta = 0.0f);

    // Rendering helpers
    void renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos);
    void renderRingGizmos(glm::vec2 resolution, const Camera &camera, class UIManager &uiManager);
    void renderAdhesionLines(glm::vec2 resolution, const Camera &camera, bool showAdhesionLines);

    // Getters (delegated to component managers)
    int getCellCount() const { return bufferManager.getCellCount(); }
    float getSpawnRadius() const { return physicsManager.getSpawnRadius(); }
    bool hasSelectedCell() const { return interactionManager.hasSelectedCell(); }
    const CellInteractionManager::SelectedCellInfo &getSelectedCell() const { return interactionManager.getSelectedCell(); }
    bool isDraggingCell() const { return interactionManager.isDraggingCell; }
    int getVisibleCellCount() const { return renderingManager.getVisibleCellCount(); }
    float getMaxRenderDistance() const { return renderingManager.getMaxRenderDistance(); }
    float getFadeStartDistance() const { return renderingManager.getFadeStartDistance(); }
    float getFadeEndDistance() const { return renderingManager.getFadeEndDistance(); }
    const CellBarrierManager::BarrierStats& getBarrierStats() const { return barrierManager.getBarrierStats(); }

    // Setters (delegated to component managers)
    void setCellLimit(int limit) { bufferManager.setCellLimit(limit); }
    void setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd) { 
        renderingManager.setDistanceCullingParams(maxDistance, fadeStart, fadeEnd); 
    }
    void setFogColor(const glm::vec3& color) { renderingManager.setFogColor(color); }

    // Cell data access
    ComputeCell getCellData(int index) const { return bufferManager.getCellData(index); }
    void updateCellData(int index, const ComputeCell &newData) { bufferManager.updateCellData(index, newData); }
    void clearSelection() { interactionManager.clearSelection(); }
    void endDrag() { interactionManager.endDrag(); }
}; 