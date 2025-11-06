#pragma once

#include <memory>
#include <vector>
#include <chrono>
#include <glm/glm.hpp>
#include "../cell/common_structs.h"
#include "cpu_soa_data_manager.h"
#include "cpu_simd_physics_engine.h"
#include "cpu_triple_buffer_system.h"

// Forward declarations
class CPUSoADataManager;
class CPUSIMDPhysicsEngine;
class CPUTripleBufferSystem;

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

    // Scene management (native SoA format)
    void createEmptyScene(size_t maxCells = 256);
    void loadPreviewScene(const std::string& filename);
    void savePreviewScene(const std::string& filename);

    // Simulation control
    void update(float deltaTime);
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

    // Performance monitoring
    float getLastSimulationTime() const { return m_lastSimulationTime; }
    size_t getActiveCellCount() const;
    size_t getActiveConnectionCount() const;

    // System boundaries - no GPU dependencies
    bool hasGPUDependencies() const { return false; }
    bool requiresDataConversion() const { return false; }

private:
    // Core CPU system components
    std::unique_ptr<CPUSoADataManager> m_dataManager;
    std::unique_ptr<CPUSIMDPhysicsEngine> m_physicsEngine;
    std::unique_ptr<CPUTripleBufferSystem> m_visualSystem;

    // System state
    bool m_initialized = false;
    bool m_enabled = true;
    float m_lastSimulationTime = 0.0f;

    // Performance monitoring
    std::chrono::steady_clock::time_point m_frameStart;
    
    // Internal methods
    void validateSystemState();
    void updatePerformanceMetrics();
};