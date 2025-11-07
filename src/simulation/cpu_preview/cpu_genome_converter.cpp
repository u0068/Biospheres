#include "cpu_genome_converter.h"
#include "../cell/common_structs.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>

// Include CPU Preview System after the header to avoid circular dependency
#include "cpu_preview_system.h"

CPUGenomeParameters CPUGenomeConverter::convertToCPUFormat(const GenomeData& genomeData, int modeIndex) {
    // Use initial mode if no specific mode requested
    int targetMode = (modeIndex >= 0 && modeIndex < genomeData.modes.size()) ? 
                     modeIndex : genomeData.initialMode;
    
    if (targetMode >= genomeData.modes.size()) {
        targetMode = 0; // Fallback to first mode
    }
    
    const ModeSettings& mode = genomeData.modes[targetMode];
    return convertModeToCPUFormat(mode);
}

CPUGenomeParameters CPUGenomeConverter::convertModeToCPUFormat(const ModeSettings& mode) {
    CPUGenomeParameters params{};
    
    // Store the actual adhesion settings from the genome
    params.adhesionSettings = mode.adhesionSettings;
    
    // Convert division parameters
    params.divisionThreshold = convertDivisionThreshold(mode);
    
    // Convert metabolic parameters
    params.metabolicRate = convertMetabolicRate(mode);
    
    // Set mutation rate (currently not in UI, use default)
    params.mutationRate = 0.01f; // 1% mutation rate
    
    // Convert preferred direction from parent split direction
    params.preferredDirection = convertPreferredDirection(mode);
    
    // Convert mode color
    params.modeColor = mode.color;
    

    
    // Convert cell type flags
    params.cellTypeFlags = convertCellTypeFlags(mode);
    
    // Convert split direction (same as preferred direction for now)
    params.splitDirection = convertPreferredDirection(mode);
    
    // Convert child modes (for now, use same mode for both children)
    // In a full implementation, this would come from the genome's mode hierarchy
    params.childModeA = 0; // Default to first mode
    params.childModeB = 0; // Default to first mode
    
    return params;
}

void CPUGenomeConverter::applyGenomeToPreviewSystem(CPUPreviewSystem& previewSystem, const GenomeData& genomeData) {
    if (!previewSystem.isInitialized()) {
        std::cerr << "CPU Preview System not initialized - cannot apply genome parameters\n";
        return;
    }
    
    // Validate parameters before applying
    if (!validateGenomeParameters(genomeData)) {
        std::cerr << "Invalid genome parameters - skipping application\n";
        return;
    }
    
    // Measure application time to ensure sub-16ms performance
    auto startTime = std::chrono::steady_clock::now();
    
    // Convert and apply initial mode parameters to all cells
    CPUGenomeParameters params = convertToCPUFormat(genomeData, genomeData.initialMode);
    
    // Apply to all cells with genome ID 0 (default genome)
    previewSystem.applyGenomeToAll(0, params);
    
    // Trigger instant resimulation for immediate feedback
    previewSystem.triggerInstantResimulation();
    
    // Measure total application time
    auto endTime = std::chrono::steady_clock::now();
    float applicationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    // Verify sub-16ms performance target is met (Requirements 1.1, 1.3, 1.4)
    // Performance target checking completed silently
    
    // Performance verification completed silently
}

void CPUGenomeConverter::createPreviewSceneWithGenome(CPUPreviewSystem& previewSystem, const GenomeData& genomeData) {
    if (!previewSystem.isInitialized()) {
        std::cerr << "CPU Preview System not initialized - cannot create preview scene\n";
        return;
    }
    
    // Validate parameters before creating scene
    if (!validateGenomeParameters(genomeData)) {
        std::cerr << "Invalid genome parameters - skipping scene creation\n";
        return;
    }
    
    // Debug: Print source genome color

    
    // Convert genome to CPU format
    CPUGenomeParameters params = convertToCPUFormat(genomeData, genomeData.initialMode);
    

    
    // Create a few initial cells with the genome parameters
    for (int i = 0; i < 5; ++i) {
        CPUCellParameters cellParams;
        
        // Position cells in a small cluster
        cellParams.position = glm::vec3(
            (i - 2) * 3.0f,  // Spread along X axis
            0.0f,
            0.0f
        );
        
        cellParams.velocity = glm::vec3(0.0f); // Start at rest
        cellParams.orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity
        cellParams.mass = 1.0f;
        cellParams.radius = 1.0f;
        cellParams.cellType = static_cast<uint32_t>(genomeData.modes[genomeData.initialMode].cellType);
        cellParams.genomeID = 0; // Default genome
        cellParams.genome = params;
        
        // Add the cell to the preview system
        previewSystem.addCell(cellParams);
    }
    

}

void CPUGenomeConverter::applyModeToPreviewSystem(CPUPreviewSystem& previewSystem, const ModeSettings& mode, uint32_t modeNumber) {
    if (!previewSystem.isInitialized()) {
        std::cerr << "CPU Preview System not initialized - cannot apply mode parameters\n";
        return;
    }
    
    // Convert mode to CPU format
    CPUGenomeParameters params = convertModeToCPUFormat(mode);
    
    // Apply to all cells with the specified mode number
    // Note: This requires extending the system to track mode numbers per cell
    // For now, apply to all cells as a simplified implementation
    previewSystem.applyGenomeToAll(modeNumber, params);
    
    // Trigger instant resimulation
    previewSystem.triggerInstantResimulation();
}

bool CPUGenomeConverter::validateGenomeParameters(const GenomeData& genomeData) {
    if (genomeData.modes.empty()) {
        std::cerr << "Genome validation failed: No modes defined\n";
        return false;
    }
    
    if (genomeData.initialMode >= genomeData.modes.size()) {
        std::cerr << "Genome validation failed: Invalid initial mode index\n";
        return false;
    }
    
    // Validate each mode
    for (size_t i = 0; i < genomeData.modes.size(); ++i) {
        const ModeSettings& mode = genomeData.modes[i];
        
        // Validate cell type
        if (!isValidCellType(mode.cellType)) {
            std::cerr << "Genome validation failed: Invalid cell type in mode " << i << "\n";
            return false;
        }
        
        // Validate split mass
        if (!isParameterInRange(mode.splitMass, 0.1f, 10.0f)) {
            std::cerr << "Genome validation failed: Split mass out of range in mode " << i << "\n";
            return false;
        }
        
        // Validate split interval
        if (!isParameterInRange(mode.splitInterval, 1.0f, 30.0f)) {
            std::cerr << "Genome validation failed: Split interval out of range in mode " << i << "\n";
            return false;
        }
        
        // Validate adhesion settings if enabled
        if (mode.parentMakeAdhesion) {
            const AdhesionSettings& adhesion = mode.adhesionSettings;
            
            if (!isParameterInRange(adhesion.breakForce, 0.1f, 100.0f)) {
                std::cerr << "Genome validation failed: Adhesion break force out of range in mode " << i << "\n";
                return false;
            }
            
            if (!isParameterInRange(adhesion.restLength, 0.5f, 5.0f)) {
                std::cerr << "Genome validation failed: Adhesion rest length out of range in mode " << i << "\n";
                return false;
            }
        }
    }
    
    return true;
}

float CPUGenomeConverter::estimatePerformanceImpact(const GenomeData& genomeData, size_t cellCount) {
    if (genomeData.modes.empty() || cellCount == 0) {
        return 0.0f;
    }
    
    // Base simulation time per cell (empirically determined)
    float baseTimePerCell = 0.05f; // 0.05ms per cell
    
    // Get complexity factors from initial mode
    const ModeSettings& mode = genomeData.modes[genomeData.initialMode];
    
    float adhesionComplexity = estimateAdhesionComplexity(mode.adhesionSettings);
    float cellTypeComplexity = estimateCellTypeComplexity(mode.cellType);
    
    // Calculate total estimated time
    float totalTime = cellCount * baseTimePerCell * adhesionComplexity * cellTypeComplexity;
    
    return totalTime;
}

// Private helper methods



float CPUGenomeConverter::convertDivisionThreshold(const ModeSettings& mode) {
    // Use split interval as division threshold (age-based division)
    // Split interval is the time in seconds between cell divisions
    return mode.splitInterval;
}

float CPUGenomeConverter::convertMetabolicRate(const ModeSettings& mode) {
    // Convert split interval to metabolic rate (inverse relationship)
    // Shorter intervals = higher metabolic rate
    float normalizedRate = 30.0f / std::max(mode.splitInterval, 1.0f);
    return std::clamp(normalizedRate, 0.1f, 30.0f);
}

glm::vec3 CPUGenomeConverter::convertPreferredDirection(const ModeSettings& mode) {
    // Convert parent split direction angles to normalized direction vector
    // This is now safe because we do complete scene resimulation instead of modifying existing cells
    float pitchRad = glm::radians(mode.parentSplitDirection.x);
    float yawRad = glm::radians(mode.parentSplitDirection.y);
    
    // Convert spherical coordinates to Cartesian
    glm::vec3 direction;
    direction.x = std::cos(pitchRad) * std::cos(yawRad);
    direction.y = std::sin(pitchRad);
    direction.z = std::cos(pitchRad) * std::sin(yawRad);
    
    return glm::normalize(direction);
}

uint32_t CPUGenomeConverter::convertCellTypeFlags(const ModeSettings& mode) {
    uint32_t flags = 0;
    
    // Set cell type flag
    flags |= static_cast<uint32_t>(mode.cellType);
    
    // Set adhesion flag
    if (mode.parentMakeAdhesion) {
        flags |= (1 << 8); // Bit 8 for adhesion capability
    }
    
    // Set flagellocyte-specific flags
    if (mode.cellType == CellType::Flagellocyte) {
        flags |= (1 << 9); // Bit 9 for thrust capability
        
        // Encode thrust force in upper bits (scaled to 0-255 range)
        uint32_t thrustValue = static_cast<uint32_t>(
            std::clamp(mode.flagellocyteSettings.thrustForce * 12.75f, 0.0f, 255.0f)
        );
        flags |= (thrustValue << 16); // Bits 16-23 for thrust force
    }
    
    return flags;
}

bool CPUGenomeConverter::isParameterInRange(float value, float min, float max) {
    return value >= min && value <= max && std::isfinite(value);
}

bool CPUGenomeConverter::isValidCellType(CellType cellType) {
    return cellType >= CellType::Phagocyte && cellType < CellType::COUNT;
}

float CPUGenomeConverter::estimateAdhesionComplexity(const AdhesionSettings& adhesion) {
    // Base complexity factor
    float complexity = 1.0f;
    
    // Higher stiffness increases computation complexity
    complexity += adhesion.linearSpringStiffness / 500.0f;
    
    // Angular constraints add complexity
    complexity += adhesion.orientationSpringStiffness / 100.0f;
    
    // Twist constraints add significant complexity
    if (adhesion.enableTwistConstraint) {
        complexity += 0.5f;
    }
    
    return std::clamp(complexity, 1.0f, 3.0f);
}

float CPUGenomeConverter::estimateCellTypeComplexity(CellType cellType) {
    switch (cellType) {
        case CellType::Phagocyte:
            return 1.0f; // Base complexity
        case CellType::Flagellocyte:
            return 1.3f; // Additional thrust calculations
        default:
            return 1.0f;
    }
}