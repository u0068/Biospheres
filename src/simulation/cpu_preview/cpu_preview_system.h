#pragma once

#include <memory>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "../cell/common_structs.h"
#include "cpu_soa_data_manager.h"
#include "cpu_simd_physics_engine.h"
#include "cpu_triple_buffer_system.h"
#include "cpu_genome_manager.h"

// Forward declarations
class CPUSoADataManager;
class CPUSIMDPhysicsEngine;
class CPUTripleBufferSystem;
class CPUGenomeManager;
class Shader;

/**
 * CPU Native Preview System - Main coordination class
 * 
 * This system provides sub-16ms cellular simulation for Preview Simulation scenes
 * using native Structure-of-Arrays (SoA) data layout optimized for CPU SIMD operations.
 * Operates completely independently from the GPU Main Simulation System.
 * 
 * Requirements addressed: 1.1, 1.2, 1.5, 3.4
 */
class CPUPreviewSystem {
public:
    CPUPreviewSystem();
    ~CPUPreviewSystem();

    // System lifecycle
    void initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // Scene management (native SoA format) - Requirements 3.2, 3.3, 3.5
    void createEmptyScene(size_t maxCells = 256);
    void loadPreviewScene(const std::string& filename);
    void savePreviewScene(const std::string& filename);
    
    // Scene file format validation
    bool isValidSoAFile(const std::string& filename) const;
    std::string getSceneFileExtension() const { return ".soa"; }

    // Simulation control
    void update(float deltaTime);
    void fastForward(float totalTime, float timeStep = 0.01f); // Fast simulation without visual updates
    void reset();
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // Genome parameter updates (instant, no conversion overhead)
    void updateGenomeParameters(uint32_t cellIndex, const CPUGenomeParameters& params);
    void applyGenomeToAll(uint32_t genomeID, const CPUGenomeParameters& params);

    // Cell management (native CPU SoA operations)
    uint32_t addCell(const CPUCellParameters& params);
    void removeCell(uint32_t cellIndex);
    void addAdhesionConnection(uint32_t cellA, uint32_t cellB, const CPUAdhesionParameters& params);

    // Visual data access for rendering pipeline
    const CPUTripleBufferSystem::CPUVisualData* getVisualData() const;
    void uploadVisualDataToGPU();
    CPUTripleBufferSystem* getTripleBufferSystem() const { return m_visualSystem.get(); }

    // Performance monitoring
    float getLastSimulationTime() const { return m_lastSimulationTime; }
    size_t getActiveCellCount() const;
    size_t getActiveConnectionCount() const;

    // System boundaries - no GPU dependencies
    bool hasGPUDependencies() const { return false; }
    bool requiresDataConversion() const { return false; }

    // Genome integration for instant iteration (Requirements 1.1, 1.2, 1.5)
    void onGenomeParametersChanged(const CPUGenomeParameters& newParams);
    void triggerInstantResimulation();
    bool isPerformanceTargetMet() const { return m_lastSimulationTime <= 16.0f; }
    
    // Preview-specific genome handling (Requirements 1.1, 1.3, 1.4)
    void setDeterministicMode(bool enabled, uint32_t seed = 12345);
    bool isDeterministicMode() const;
    CPUGenomeManager* getGenomeManager() const { return m_genomeManager.get(); }
    
    // Visual data management
    void ensureVisualDataCurrent();
    
    // Gizmo rendering for CPU Preview System (uses existing GPU gizmo system)
    void renderGizmos(glm::vec2 resolution, const class Camera& camera, bool showGizmos);
    void renderRingGizmos(glm::vec2 resolution, const class Camera& camera, const class UIManager& uiManager);
    void renderAnchorGizmos(glm::vec2 resolution, const class Camera& camera, const class UIManager& uiManager);
    void renderAdhesionLines(glm::vec2 resolution, const class Camera& camera, bool showAdhesionLines);

private:
    // Core CPU system components
    std::unique_ptr<CPUSoADataManager> m_dataManager;
    std::unique_ptr<CPUSIMDPhysicsEngine> m_physicsEngine;
    std::unique_ptr<CPUTripleBufferSystem> m_visualSystem;
    std::unique_ptr<CPUGenomeManager> m_genomeManager;

    // System state
    bool m_initialized = false;
    bool m_enabled = true;
    float m_lastSimulationTime = 0.0f;
    bool m_suppressVisualUpdates = false; // Flag to suppress visual updates during fast-forward
    
    // Current genome parameters for division logic
    CPUGenomeParameters m_currentGenomeParams;
    std::vector<GPUModeAdhesionSettings> m_cachedModeSettings; // Cached to avoid repeated allocations

    // Performance monitoring
    std::chrono::steady_clock::time_point m_frameStart;
    
    // Visual update rate limiting (60 FPS max)
    std::chrono::steady_clock::time_point m_lastVisualUpdate;
    static constexpr float TARGET_VISUAL_FPS = 60.0f;
    static constexpr float MIN_VISUAL_UPDATE_INTERVAL = 1.0f / TARGET_VISUAL_FPS; // ~16.67ms
    
    // GPU gizmo system integration (reuses existing GPU gizmo infrastructure)
    GLuint m_gpuCellBuffer = 0;
    GLuint m_gpuModeBuffer = 0;
    GLuint m_gpuCellCountBuffer = 0;
    GLuint m_gpuAdhesionBuffer = 0;  // GPU buffer for adhesion connections (for anchor gizmo rendering)
    
    // Gizmo rendering infrastructure (shared with CellManager)
    class Shader* m_gizmoExtractShader = nullptr;
    class Shader* m_gizmoShader = nullptr;
    class Shader* m_ringGizmoExtractShader = nullptr;
    class Shader* m_ringGizmoShader = nullptr;
    class Shader* m_anchorGizmoExtractShader = nullptr;
    class Shader* m_anchorGizmoShader = nullptr;
    class Shader* m_adhesionLineExtractShader = nullptr;
    class Shader* m_adhesionLineShader = nullptr;
    
    // Separate sphere mesh for anchor gizmo rendering (to avoid conflicts with cell rendering)
    std::unique_ptr<class SphereMesh> m_anchorSphereMesh;
    
    // Gizmo GPU buffers and VAOs
    GLuint m_gizmoBuffer = 0;
    GLuint m_gizmoVAO = 0;
    GLuint m_gizmoVBO = 0;
    GLuint m_ringGizmoBuffer = 0;
    GLuint m_ringGizmoVAO = 0;
    GLuint m_ringGizmoVBO = 0;
    GLuint m_anchorGizmoBuffer = 0;
    GLuint m_anchorGizmoVBO = 0;
    GLuint m_anchorCountBuffer = 0;
    uint32_t m_totalAnchorCount = 0;
    GLuint m_adhesionLineBuffer = 0;
    GLuint m_adhesionLineVAO = 0;
    GLuint m_adhesionLineVBO = 0;
    
    // Internal methods
    void validateSystemState();
    void updatePerformanceMetrics();
    void initializeGPUBuffers();
    void cleanupGPUBuffers();
    void uploadCellDataToGPU();
    void uploadAdhesionDataToGPU();
    
    // Mode settings conversion (Requirements 4.5)
    std::vector<GPUModeAdhesionSettings> createModeSettingsFromGenome(const CPUGenomeParameters& genomeParams);
    
    // Gizmo system methods (reuse existing GPU gizmo infrastructure)
    void initializeGizmoSystem();
    void cleanupGizmoSystem();
    void initializeGizmoBuffers();
    void initializeRingGizmoBuffers();
    void initializeAnchorGizmoBuffers();
    void initializeAdhesionLineBuffers();
    void updateGizmoData();
    void updateRingGizmoData();
    void updateAnchorGizmoData();
    void updateAdhesionLineData();
    void cleanupGizmos();
    void cleanupRingGizmos();
    void cleanupAnchorGizmos();
    void cleanupAdhesionLines();
};