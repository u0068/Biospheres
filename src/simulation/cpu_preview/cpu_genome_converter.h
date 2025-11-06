#pragma once

#include "../cell/common_structs.h"
#include "cpu_soa_data_manager.h"
#include <glm/glm.hpp>

// Forward declaration
class CPUPreviewSystem;

/**
 * CPU Genome Parameter Converter
 * 
 * Converts between UI GenomeData format and CPU Preview System's CPUGenomeParameters.
 * Enables instant genome parameter updates without GPU dependency.
 * 
 * Requirements addressed: 1.1, 1.2, 1.5
 */
class CPUGenomeConverter {
public:
    /**
     * Convert UI GenomeData to CPU Preview System format
     * 
     * Extracts relevant parameters from the complex UI genome structure
     * and converts them to the optimized CPU format for instant updates.
     */
    static CPUGenomeParameters convertToCPUFormat(const GenomeData& genomeData, int modeIndex = -1);
    
    /**
     * Convert specific mode settings to CPU format
     * 
     * Converts a specific mode from the genome to CPU parameters.
     * Used for mode-specific parameter updates.
     */
    static CPUGenomeParameters convertModeToCPUFormat(const ModeSettings& mode);
    
    /**
     * Apply genome parameters to CPU Preview System
     * 
     * Triggers instant resimulation with new parameters.
     * Ensures sub-16ms performance target is met.
     */
    static void applyGenomeToPreviewSystem(CPUPreviewSystem& previewSystem, const GenomeData& genomeData);
    
    /**
     * Create a fresh preview scene with genome parameters
     * 
     * Creates initial cells with the specified genome parameters.
     * Used for complete scene resimulation to prevent data corruption.
     */
    static void createPreviewSceneWithGenome(CPUPreviewSystem& previewSystem, const GenomeData& genomeData);
    
    /**
     * Apply specific mode parameters to all cells of that mode
     * 
     * Updates only cells that are currently in the specified mode.
     * Enables selective parameter updates for efficient iteration.
     */
    static void applyModeToPreviewSystem(CPUPreviewSystem& previewSystem, const ModeSettings& mode, uint32_t modeNumber);
    
    /**
     * Validate genome parameters for CPU Preview System
     * 
     * Ensures parameters are within acceptable ranges for CPU simulation.
     * Returns true if parameters are valid, false otherwise.
     */
    static bool validateGenomeParameters(const GenomeData& genomeData);
    
    /**
     * Get performance impact estimate
     * 
     * Estimates the performance impact of applying these genome parameters.
     * Returns estimated simulation time in milliseconds.
     */
    static float estimatePerformanceImpact(const GenomeData& genomeData, size_t cellCount);

private:
    // Parameter conversion helpers
    static float convertAdhesionStrength(const AdhesionSettings& adhesion);
    static float convertDivisionThreshold(const ModeSettings& mode);
    static float convertMetabolicRate(const ModeSettings& mode);
    static glm::vec3 convertPreferredDirection(const ModeSettings& mode);
    static uint32_t convertCellTypeFlags(const ModeSettings& mode);
    
    // Validation helpers
    static bool isParameterInRange(float value, float min, float max);
    static bool isValidCellType(CellType cellType);
    
    // Performance estimation helpers
    static float estimateAdhesionComplexity(const AdhesionSettings& adhesion);
    static float estimateCellTypeComplexity(CellType cellType);
};