#include "cpu_genome_manager.h"
#include "../cell/common_structs.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <chrono>

CPUGenomeManager::CPUGenomeManager() 
    : m_deterministicRng(m_deterministicSeed) {
}

void CPUGenomeManager::initialize(uint32_t deterministicSeed) {
    m_deterministicSeed = deterministicSeed;
    m_deterministicRng.seed(deterministicSeed);
    m_deterministicMode = true; // Enable deterministic mode by default for genome iteration
    
    // Clear application history
    m_applicationHistory.clear();
    m_statsValid = false;
    
    // CPU Genome Manager initialized silently
}

void CPUGenomeManager::applyGenomeToSoAData(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params) {
    auto startTime = std::chrono::steady_clock::now();
    
    // Validate parameters before application
    if (!validateGenomeForSoA(params)) {
        std::cerr << "Invalid genome parameters - skipping application to SoA data\n";
        return;
    }
    
    // Apply parameters to all active cells
    for (size_t i = 0; i < cellData.activeCellCount; ++i) {
        applyCellTypeParameters(cellData, params, static_cast<uint32_t>(i));
        applyAdhesionParameters(cellData, params, static_cast<uint32_t>(i));
        applyMetabolicParameters(cellData, params, static_cast<uint32_t>(i));
        applyDirectionalParameters(cellData, params, static_cast<uint32_t>(i));
        applyColorParameters(cellData, params, static_cast<uint32_t>(i));
    }
    
    // Record application performance
    auto endTime = std::chrono::steady_clock::now();
    float applicationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    recordGenomeApplication(params, applicationTime);
    
    // Genome parameters applied successfully
}

void CPUGenomeManager::applyGenomeToMode(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t modeNumber) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!validateGenomeForSoA(params)) {
        std::cerr << "Invalid genome parameters - skipping mode application\n";
        return;
    }
    
    size_t affectedCells = 0;
    
    // Apply parameters only to cells with matching mode number
    // Note: Mode number is encoded in the genomeID field for simplicity
    for (size_t i = 0; i < cellData.activeCellCount; ++i) {
        if (cellData.genomeID[i] == modeNumber) {
            applyCellTypeParameters(cellData, params, static_cast<uint32_t>(i));
            applyAdhesionParameters(cellData, params, static_cast<uint32_t>(i));
            applyMetabolicParameters(cellData, params, static_cast<uint32_t>(i));
            applyDirectionalParameters(cellData, params, static_cast<uint32_t>(i));
            applyColorParameters(cellData, params, static_cast<uint32_t>(i));
            affectedCells++;
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    float applicationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    recordGenomeApplication(params, applicationTime);
    
    // Genome parameters applied to mode successfully
}

void CPUGenomeManager::applyGenomeToIndices(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, const std::vector<uint32_t>& indices) {
    auto startTime = std::chrono::steady_clock::now();
    
    if (!validateGenomeForSoA(params)) {
        std::cerr << "Invalid genome parameters - skipping indexed application\n";
        return;
    }
    
    // Apply parameters to specified cell indices
    for (uint32_t index : indices) {
        if (index < cellData.activeCellCount) {
            applyCellTypeParameters(cellData, params, index);
            applyAdhesionParameters(cellData, params, index);
            applyMetabolicParameters(cellData, params, index);
            applyDirectionalParameters(cellData, params, index);
            applyColorParameters(cellData, params, index);
        }
    }
    
    auto endTime = std::chrono::steady_clock::now();
    float applicationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
    
    recordGenomeApplication(params, applicationTime);
    
    // Genome parameters applied to specified cells successfully
}

void CPUGenomeManager::setDeterministicMode(bool enabled, uint32_t seed) {
    m_deterministicMode = enabled;
    if (enabled) {
        m_deterministicSeed = seed;
        m_deterministicRng.seed(seed);
        // Deterministic mode enabled
    } else {
        // Use current time as seed for non-deterministic behavior
        auto currentTime = std::chrono::steady_clock::now().time_since_epoch().count();
        m_deterministicRng.seed(static_cast<uint32_t>(currentTime));
        // Deterministic mode disabled
    }
}

float CPUGenomeManager::getDeterministicRandom() {
    if (m_deterministicMode) {
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(m_deterministicRng);
    } else {
        // Use system random for non-deterministic behavior
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return std::uniform_real_distribution<float>(0.0f, 1.0f)(gen);
    }
}

uint32_t CPUGenomeManager::getDeterministicRandomInt(uint32_t min, uint32_t max) {
    if (m_deterministicMode) {
        return std::uniform_int_distribution<uint32_t>(min, max)(m_deterministicRng);
    } else {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        return std::uniform_int_distribution<uint32_t>(min, max)(gen);
    }
}

bool CPUGenomeManager::validateGenomeForSoA(const CPUGenomeParameters& params) {
    // Validate adhesion settings
    if (!isValidAdhesionSettings(params.adhesionSettings)) {
        std::cerr << "Invalid adhesion settings" << std::endl;
        return false;
    }
    
    // Validate division threshold
    if (!isValidDivisionThreshold(params.divisionThreshold)) {
        std::cerr << "Invalid division threshold: " << params.divisionThreshold << std::endl;
        return false;
    }
    
    // Validate metabolic rate
    if (!isValidMetabolicRate(params.metabolicRate)) {
        std::cerr << "Invalid metabolic rate: " << params.metabolicRate << std::endl;
        return false;
    }
    
    // Validate cell type flags
    if (!isValidCellTypeFlags(params.cellTypeFlags)) {
        std::cerr << "Invalid cell type flags: " << params.cellTypeFlags << std::endl;
        return false;
    }
    
    // Validate preferred direction (should be normalized)
    float directionLength = glm::length(params.preferredDirection);
    if (directionLength < 0.9f || directionLength > 1.1f) {
        std::cerr << "Invalid preferred direction (not normalized): length = " << directionLength << std::endl;
        return false;
    }
    
    return true;
}

float CPUGenomeManager::estimateApplicationTime(const CPUGenomeParameters& params, size_t cellCount) {
    if (cellCount == 0) {
        return 0.0f;
    }
    
    // Base time per cell (empirically determined)
    float baseTimePerCell = 0.001f; // 0.001ms per cell for parameter application
    
    // Calculate complexity factors
    float cellTypeComplexity = estimateCellTypeComplexity(params.cellTypeFlags);
    float adhesionComplexity = estimateAdhesionComplexity(params.adhesionSettings);
    float metabolicComplexity = estimateMetabolicComplexity(params.metabolicRate);
    
    // Total estimated time
    float totalTime = cellCount * baseTimePerCell * cellTypeComplexity * adhesionComplexity * metabolicComplexity;
    
    return totalTime;
}

void CPUGenomeManager::recordGenomeApplication(const CPUGenomeParameters& params, float applicationTime) {
    GenomeApplicationRecord record;
    record.parameters = params;
    record.timestamp = std::chrono::steady_clock::now();
    record.applicationTime = applicationTime;
    record.affectedCells = 0; // Will be set by caller if needed
    
    m_applicationHistory.push_back(record);
    
    // Limit history size
    if (m_applicationHistory.size() > MAX_HISTORY_SIZE) {
        m_applicationHistory.erase(m_applicationHistory.begin());
    }
    
    // Invalidate cached statistics
    m_statsValid = false;
}

CPUGenomeManager::GenomeApplicationStats CPUGenomeManager::getApplicationStats() const {
    if (!m_statsValid) {
        updateCachedStats();
    }
    return m_cachedStats;
}

void CPUGenomeManager::resetApplicationHistory() {
    m_applicationHistory.clear();
    m_statsValid = false;
    // Genome application history reset
}

// Private methods

void CPUGenomeManager::applyCellTypeParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex) {
    // Extract cell type from flags
    uint32_t cellType = params.cellTypeFlags & 0xFF;
    cellData.cellType[cellIndex] = cellType;
    
    // Apply cell type-specific parameters
    if (cellType == static_cast<uint32_t>(CellType::Flagellocyte)) {
        // Extract thrust force from upper bits of flags
        uint32_t thrustValue = (params.cellTypeFlags >> 16) & 0xFF;
        float thrustForce = thrustValue / 12.75f; // Convert back from encoded value
        
        // Apply thrust as additional energy (simplified implementation)
        cellData.energy[cellIndex] += thrustForce * 0.1f;
    }
}

void CPUGenomeManager::applyAdhesionParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex) {
    // Use standard cell radius (no adhesion-based modification needed)
    float baseRadius = 1.0f; // Standard cell radius
    cellData.radius[cellIndex] = baseRadius;
}

void CPUGenomeManager::applyMetabolicParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex) {
    // Set metabolic properties based on genome (don't accumulate to prevent growth from UI changes)
    // These should be SET to genome-defined values, not added to existing values
    
    // Reset to base values and apply genome modifiers
    cellData.mass[cellIndex] = 1.0f; // Reset to base mass
    cellData.age[cellIndex] = 0.0f;  // Reset age when genome changes
    
    // Apply genome-specific modifiers (these are now deterministic, not accumulative)
    // The actual metabolic effects should happen in the physics simulation, not here
}

void CPUGenomeManager::applyDirectionalParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex) {
    // Apply preferred direction as a bias to velocity
    // This is safe now because we do complete scene resimulation instead of modifying existing cells
    float biasStrength = 0.1f; // Small directional bias
    
    // Set the velocity bias for new cells created during scene resimulation
    cellData.vel_x[cellIndex] = params.preferredDirection.x * biasStrength;
    cellData.vel_y[cellIndex] = params.preferredDirection.y * biasStrength;
    cellData.vel_z[cellIndex] = params.preferredDirection.z * biasStrength;
}

void CPUGenomeManager::applyColorParameters(CPUCellPhysics_SoA& cellData, const CPUGenomeParameters& params, uint32_t cellIndex) {
    // Apply genome mode color directly to cell
    cellData.color_r[cellIndex] = params.modeColor.r;
    cellData.color_g[cellIndex] = params.modeColor.g;
    cellData.color_b[cellIndex] = params.modeColor.b;
}

// Validation helpers

bool CPUGenomeManager::isValidAdhesionSettings(const AdhesionSettings& settings) const {
    return settings.breakForce >= 0.0f && std::isfinite(settings.breakForce) &&
           settings.restLength >= 0.0f && std::isfinite(settings.restLength) &&
           settings.linearSpringStiffness >= 0.0f && std::isfinite(settings.linearSpringStiffness) &&
           settings.linearSpringDamping >= 0.0f && std::isfinite(settings.linearSpringDamping) &&
           settings.orientationSpringStiffness >= 0.0f && std::isfinite(settings.orientationSpringStiffness) &&
           settings.orientationSpringDamping >= 0.0f && std::isfinite(settings.orientationSpringDamping) &&
           settings.twistConstraintStiffness >= 0.0f && std::isfinite(settings.twistConstraintStiffness) &&
           settings.twistConstraintDamping >= 0.0f && std::isfinite(settings.twistConstraintDamping);
}

bool CPUGenomeManager::isValidDivisionThreshold(float threshold) const {
    return threshold >= 0.1f && threshold <= 10.0f && std::isfinite(threshold);
}

bool CPUGenomeManager::isValidMetabolicRate(float rate) const {
    return rate >= 0.1f && rate <= 30.0f && std::isfinite(rate);
}

bool CPUGenomeManager::isValidCellTypeFlags(uint32_t flags) const {
    // Extract cell type from lower 8 bits
    uint32_t cellType = flags & 0xFF;
    return cellType < static_cast<uint32_t>(CellType::COUNT);
}

// Performance estimation helpers

float CPUGenomeManager::estimateCellTypeComplexity(uint32_t cellTypeFlags) const {
    uint32_t cellType = cellTypeFlags & 0xFF;
    
    switch (cellType) {
        case static_cast<uint32_t>(CellType::Phagocyte):
            return 1.0f;
        case static_cast<uint32_t>(CellType::Flagellocyte):
            return 1.2f; // Additional thrust calculations
        default:
            return 1.0f;
    }
}

float CPUGenomeManager::estimateAdhesionComplexity(const AdhesionSettings& settings) const {
    // Higher adhesion stiffness increases collision detection complexity
    float normalizedStiffness = std::min(settings.linearSpringStiffness / 500.0f, 1.0f);
    return 1.0f + (normalizedStiffness * 0.3f);
}

float CPUGenomeManager::estimateMetabolicComplexity(float metabolicRate) const {
    // Higher metabolic rate increases update frequency
    return 1.0f + (metabolicRate / 30.0f * 0.2f);
}

void CPUGenomeManager::updateCachedStats() const {
    if (m_applicationHistory.empty()) {
        m_cachedStats = {};
        m_statsValid = true;
        return;
    }
    
    m_cachedStats.totalApplications = m_applicationHistory.size();
    
    float totalTime = 0.0f;
    float minTime = std::numeric_limits<float>::max();
    float maxTime = 0.0f;
    size_t sub16msCount = 0;
    
    for (const auto& record : m_applicationHistory) {
        totalTime += record.applicationTime;
        minTime = std::min(minTime, record.applicationTime);
        maxTime = std::max(maxTime, record.applicationTime);
        
        if (record.applicationTime <= 16.0f) {
            sub16msCount++;
        }
    }
    
    m_cachedStats.averageApplicationTime = totalTime / m_applicationHistory.size();
    m_cachedStats.minApplicationTime = minTime;
    m_cachedStats.maxApplicationTime = maxTime;
    m_cachedStats.sub16msApplications = sub16msCount;
    m_cachedStats.performanceTargetHitRate = static_cast<float>(sub16msCount) / m_applicationHistory.size();
    
    m_statsValid = true;
}