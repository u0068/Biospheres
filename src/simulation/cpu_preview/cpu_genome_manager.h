#pragma once

#include "../cell/common_structs.h"
#include "cpu_soa_data_manager.h"
#include <unordered_map>
#include <vector>
#include <random>
#include <chrono>

/**
 * CPU Preview Genome Manager
 * 
 * Manages genome parameter application for SoA data structures.
 * Provides deterministic simulation support for genome iteration.
 * Ensures genome changes trigger sub-16ms resimulation.
 * 
 * Requirements addressed: 1.1, 1.3, 1.4
 */
class CPUGenomeManager {
public:
    CPUGenomeManager();
    ~CPUGenomeManager() = default;

    /**
     * Initialize genome manager with deterministic seed
     * 
     * Sets up deterministic random number generation for reproducible results.
     * Essential for scientific validation and genome iteration.
     */
    void initialize(uint32_t deterministicSeed = 12345);
    
    /**
     * Apply genome parameters to SoA data structures
     * 
     * Directly modifies SoA arrays for optimal CPU performance.
     * No conversion overhead - operates on native CPU data format.
     */
    void applyGenomeToSoAData(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params);
    
    /**
     * Apply genome parameters to specific cells by mode
     * 
     * Updates only cells that match the specified mode number.
     * Enables selective parameter updates for complex genomes.
     */
    void applyGenomeToMode(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t modeNumber);
    
    /**
     * Apply genome parameters to specific cell indices
     * 
     * Updates a specific set of cells with new parameters.
     * Used for targeted parameter updates during genome iteration.
     */
    void applyGenomeToIndices(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, const std::vector<uint32_t>& indices);
    
    /**
     * Set deterministic mode for reproducible simulations
     * 
     * Enables/disables deterministic behavior for genome iteration.
     * When enabled, all random operations use fixed seeds.
     */
    void setDeterministicMode(bool enabled, uint32_t seed = 12345);
    
    /**
     * Get deterministic random value
     * 
     * Returns reproducible random values for deterministic simulation.
     * Used for mutation, division timing, and other stochastic processes.
     */
    float getDeterministicRandom();
    uint32_t getDeterministicRandomInt(uint32_t min, uint32_t max);
    
    /**
     * Validate genome parameters for SoA application
     * 
     * Ensures parameters are compatible with CPU SoA data structures.
     * Validates ranges and constraints for optimal performance.
     */
    bool validateGenomeForSoA(const CPUGenomeParameters& params);
    
    /**
     * Estimate performance impact of genome application
     * 
     * Predicts simulation time impact of applying genome parameters.
     * Helps ensure sub-16ms performance target is maintained.
     */
    float estimateApplicationTime(const CPUGenomeParameters& params, size_t cellCount);
    
    /**
     * Track genome application history
     * 
     * Maintains history of genome parameter changes for analysis.
     * Enables rollback and performance trend analysis.
     */
    void recordGenomeApplication(const CPUGenomeParameters& params, float applicationTime);
    
    /**
     * Get genome application statistics
     * 
     * Returns performance statistics for genome parameter applications.
     * Used for optimization and performance monitoring.
     */
    struct GenomeApplicationStats {
        size_t totalApplications;
        float averageApplicationTime;
        float minApplicationTime;
        float maxApplicationTime;
        size_t sub16msApplications;
        float performanceTargetHitRate;
    };
    
    GenomeApplicationStats getApplicationStats() const;
    
    /**
     * Reset genome application history
     * 
     * Clears performance statistics and application history.
     * Used for fresh performance measurements.
     */
    void resetApplicationHistory();

private:
    // Deterministic simulation support
    bool m_deterministicMode = false;
    uint32_t m_deterministicSeed = 12345;
    std::mt19937 m_deterministicRng;
    
    // Genome application history
    struct GenomeApplicationRecord {
        CPUGenomeParameters parameters;
        std::chrono::steady_clock::time_point timestamp;
        float applicationTime;
        size_t affectedCells;
    };
    
    std::vector<GenomeApplicationRecord> m_applicationHistory;
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
    
    // Performance tracking
    mutable GenomeApplicationStats m_cachedStats;
    mutable bool m_statsValid = false;
    
    // Internal methods
    void applyCellTypeParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex);
    void applyAdhesionParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex);
    void applyMetabolicParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex);
    void applyDirectionalParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex);
    void applyColorParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex);
    
    // Validation helpers
    bool isValidAdhesionStrength(float strength) const;
    bool isValidDivisionThreshold(float threshold) const;
    bool isValidMetabolicRate(float rate) const;
    bool isValidCellTypeFlags(uint32_t flags) const;
    
    // Performance estimation helpers
    float estimateCellTypeComplexity(uint32_t cellTypeFlags) const;
    float estimateAdhesionComplexity(float adhesionStrength) const;
    float estimateMetabolicComplexity(float metabolicRate) const;
    
    // Statistics calculation
    void updateCachedStats() const;
};