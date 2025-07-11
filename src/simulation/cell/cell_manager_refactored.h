#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <cstddef>

#include "../../rendering/core/shader_class.h"
#include "../../input/input.h"
#include "../../core/config.h"
#include "../../rendering/core/mesh/sphere_mesh.h"
#include "../cell/common_structs.h"
#include "../../rendering/systems/frustum_culling.h"

// Include all the refactored systems
#include "cell_buffer_manager.h"
#include "cell_shader_manager.h"
#include "cell_spatial_grid.h"
#include "cell_lod_system.h"
#include "cell_culling_system.h"
#include "cell_gizmo_system.h"
#include "cell_interaction_system.h"

// Forward declarations
class Camera;
class UIManager;

// Ensure struct alignment is correct for GPU usage
static_assert(sizeof(ComputeCell) % 16 == 0, "ComputeCell must be 16-byte aligned for GPU usage");
static_assert(sizeof(GPUMode) % 16 == 16, "GPUMode must be 16-byte aligned for GPU usage");

struct CellManagerRefactored
{
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates

    // Refactored systems
    CellBufferManager bufferManager;
    CellShaderManager shaderManager;
    CellSpatialGrid spatialGrid;
    CellLODSystem lodSystem;
    CellCullingSystem cullingSystem;
    CellGizmoSystem gizmoSystem;
    CellInteractionSystem interactionSystem;

    // Barrier optimization system
    struct BarrierStats {
        int totalBarriers = 0;
        int batchedBarriers = 0;
        int flushCalls = 0;
        float barrierEfficiency = 0.0f; // batchedBarriers / totalBarriers
        
        void reset() {
            totalBarriers = 0;
            batchedBarriers = 0;
            flushCalls = 0;
            barrierEfficiency = 0.0f;
        }
        
        void updateEfficiency() {
            if (totalBarriers > 0) {
                barrierEfficiency = static_cast<float>(batchedBarriers) / static_cast<float>(totalBarriers);
            }
        }
    };

    struct BarrierBatch {
        GLbitfield pendingBarriers = 0;
        bool needsFlush = false;
        mutable BarrierStats* stats = nullptr; // Reference to stats for tracking
        
        void addBarrier(GLbitfield barrier) {
            pendingBarriers |= barrier;
            if (stats) {
                stats->totalBarriers++;
                if (pendingBarriers != barrier) {
                    stats->batchedBarriers++; // This barrier was batched with others
                }
            }
        }
        
        void flush() {
            if (pendingBarriers != 0) {
                glMemoryBarrier(pendingBarriers);
                pendingBarriers = 0;
                needsFlush = false;
                if (stats) {
                    stats->flushCalls++;
                }
            }
        }
        
        void clear() {
            pendingBarriers = 0;
            needsFlush = false;
        }
        
        void setStats(BarrierStats* statsPtr) {
            stats = statsPtr;
        }
    };

    mutable BarrierStats barrierStats;
    mutable BarrierBatch barrierBatch;

    // Constructor and destructor
    CellManagerRefactored();
    ~CellManagerRefactored();

    // Main update and rendering functions
    void updateCells(float deltaTime);
    void renderCells(glm::vec2 resolution, Shader &cellShader, Camera &camera, bool wireframe);
    void spawnCells(int count = CellBufferManager::DEFAULT_CELL_COUNT);

    // Cell addition functions (delegated to buffer manager)
    void addCellsToGPUBuffer(const std::vector<ComputeCell> &cells) { bufferManager.addCellsToGPUBuffer(cells); }
    void addCellToGPUBuffer(const ComputeCell &newCell) { bufferManager.addCellToGPUBuffer(newCell); }
    void addCellToStagingBuffer(const ComputeCell &newCell) { bufferManager.addCellToStagingBuffer(newCell); }
    void addCell(const ComputeCell &newCell) { bufferManager.addCell(newCell); }
    void addStagedCellsToGPUBuffer() { bufferManager.addStagedCellsToGPUBuffer(); }
    void addGenomeToBuffer(GenomeData& genomeData) { bufferManager.addGenomeToBuffer(genomeData); }

    // Buffer management (delegated to buffer manager)
    void resetSimulation() { bufferManager.resetSimulation(); }
    void cleanup() { bufferManager.cleanup(); }

    // Cell data access (delegated to buffer manager)
    ComputeCell getCellData(int index) const { return bufferManager.getCellData(index); }
    void updateCellData(int index, const ComputeCell &newData) { bufferManager.updateCellData(index, newData); }
    void syncCellPositionsFromGPU() { bufferManager.syncCellPositionsFromGPU(); }

    // Configuration getters (delegated to buffer manager)
    int getCellCount() const { return bufferManager.getCellCount(); }
    float getSpawnRadius() const { return bufferManager.getSpawnRadius(); }
    int getCellLimit() const { return bufferManager.getCellLimit(); }
    void setCellLimit(int limit) { bufferManager.setCellLimit(limit); }

    // Direct buffer restoration (delegated to buffer manager)
    void restoreCellsDirectlyToGPUBuffer(const std::vector<ComputeCell> &cells) { bufferManager.restoreCellsDirectlyToGPUBuffer(cells); }
    void setCPUCellData(const std::vector<ComputeCell> &cells) { bufferManager.setCPUCellData(cells); }

    // Buffer rotation and access (delegated to buffer manager)
    int getRotatedIndex(int index, int max) const { return bufferManager.getRotatedIndex(index, max); }
    void rotateBuffers() { bufferManager.rotateBuffers(); }
    GLuint getCellReadBuffer() const { return bufferManager.getCellReadBuffer(); }
    GLuint getCellWriteBuffer() const { return bufferManager.getCellWriteBuffer(); }

    // LOD system functions (delegated to LOD system)
    void updateLODLevels(const Camera& camera) { lodSystem.updateLODLevels(camera); }
    void renderCellsLOD(glm::vec2 resolution, const Camera& camera, bool wireframe = false) { lodSystem.renderCellsLOD(resolution, camera, wireframe); }
    void runLODCompute(const Camera& camera) { lodSystem.runLODCompute(camera); }
    int getTotalTriangleCount() const { return lodSystem.getTotalTriangleCount(); }
    int getTotalVertexCount() const { return lodSystem.getTotalVertexCount(); }

    // Culling system functions (delegated to culling system)
    void updateFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane) { cullingSystem.updateFrustum(camera, fov, aspectRatio, nearPlane, farPlane); }
    void runUnifiedCulling(const Camera& camera) { cullingSystem.runUnifiedCulling(camera); }
    void renderCellsUnified(glm::vec2 resolution, const Camera& camera, bool wireframe = false) { cullingSystem.renderCellsUnified(resolution, camera, wireframe); }
    void setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd) { cullingSystem.setDistanceCullingParams(maxDistance, fadeStart, fadeEnd); }
    int getVisibleCellCount() const { return cullingSystem.getVisibleCellCount(); }
    float getMaxRenderDistance() const { return cullingSystem.getMaxRenderDistance(); }
    float getFadeStartDistance() const { return cullingSystem.getFadeStartDistance(); }
    float getFadeEndDistance() const { return cullingSystem.getFadeEndDistance(); }
    void setFogColor(const glm::vec3& color) { cullingSystem.setFogColor(color); }

    // Gizmo system functions (delegated to gizmo system)
    void renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos) { gizmoSystem.renderGizmos(resolution, camera, showGizmos); }
    void renderRingGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager) { gizmoSystem.renderRingGizmos(resolution, camera, uiManager); }
    void renderAdhesionLines(glm::vec2 resolution, const Camera& camera, bool showAdhesionLines) { gizmoSystem.renderAdhesionLines(resolution, camera, showAdhesionLines); }

    // Interaction system functions (delegated to interaction system)
    void handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize, const Camera &camera, bool isMousePressed, bool isMouseDown, float scrollDelta = 0.0f) { interactionSystem.handleMouseInput(mousePos, screenSize, camera, isMousePressed, isMouseDown, scrollDelta); }
    bool hasSelectedCell() const { return interactionSystem.hasSelectedCell(); }
    const CellInteractionSystem::SelectedCellInfo &getSelectedCell() const { return interactionSystem.getSelectedCell(); }

    // Physics computation functions (delegated to shader manager)
    void runPhysicsCompute(float deltaTime) { shaderManager.runPhysicsCompute(deltaTime); }
    void runUpdateCompute(float deltaTime) { shaderManager.runUpdateCompute(deltaTime); }
    void runInternalUpdateCompute(float deltaTime) { shaderManager.runInternalUpdateCompute(deltaTime); }
    void applyCellAdditions() { bufferManager.applyCellAdditions(); }

    // Spatial grid functions (delegated to spatial grid)
    void updateSpatialGrid() { spatialGrid.updateSpatialGrid(); }

    // Barrier optimization functions
    void addBarrier(GLbitfield barrier) const { barrierBatch.addBarrier(barrier); }
    void flushBarriers() const { barrierBatch.flush(); }
    void clearBarriers() const { barrierBatch.clear(); }
    const BarrierStats& getBarrierStats() const { return barrierStats; }
    void resetBarrierStats() const { barrierStats.reset(); }

    // CPU-side storage access (delegated to buffer manager)
    const std::vector<ComputeCell>& getCPUCells() const { return bufferManager.cpuCells; }
    const std::vector<ComputeCell>& getCellStagingBuffer() const { return bufferManager.cellStagingBuffer; }
}; 