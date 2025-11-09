#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <memory>
#include <glm/glm.hpp>
#include "../cell/common_structs.h"

// Forward declarations
class CPUAdhesionConnectionManager;

/**
 * CPU Structure-of-Arrays (SoA) Data Structures
 * 
 * Optimized for CPU cache performance and SIMD operations.
 * 32-byte alignment enables AVX2 SIMD operations on 8 floats simultaneously.
 * Zero padding waste through optimal field ordering.
 * 
 * Requirements addressed: 4.1, 4.4
 */

// Core CPU SoA data structure for cell physics
struct CPUCellPhysics_SoA {
    // Position data (SIMD-aligned for vec3 operations)
    alignas(32) std::array<float, 256> pos_x;
    alignas(32) std::array<float, 256> pos_y;
    alignas(32) std::array<float, 256> pos_z;
    
    // Velocity data
    alignas(32) std::array<float, 256> vel_x;
    alignas(32) std::array<float, 256> vel_y;
    alignas(32) std::array<float, 256> vel_z;
    
    // Acceleration data
    alignas(32) std::array<float, 256> acc_x;
    alignas(32) std::array<float, 256> acc_y;
    alignas(32) std::array<float, 256> acc_z;
    
    // Previous acceleration (for Verlet integration)
    alignas(32) std::array<float, 256> prevAcc_x;
    alignas(32) std::array<float, 256> prevAcc_y;
    alignas(32) std::array<float, 256> prevAcc_z;
    
    // Orientation (quaternion) - affected by physics
    alignas(32) std::array<float, 256> quat_x;
    alignas(32) std::array<float, 256> quat_y;
    alignas(32) std::array<float, 256> quat_z;
    alignas(32) std::array<float, 256> quat_w;

    // Genome orientation (quaternion) - NEVER affected by physics, used for twist references
    alignas(32) std::array<float, 256> genomeQuat_x;
    alignas(32) std::array<float, 256> genomeQuat_y;
    alignas(32) std::array<float, 256> genomeQuat_z;
    alignas(32) std::array<float, 256> genomeQuat_w;

    // Angular motion (for complete adhesion physics)
    alignas(32) std::array<float, 256> angularVel_x;
    alignas(32) std::array<float, 256> angularVel_y;
    alignas(32) std::array<float, 256> angularVel_z;
    alignas(32) std::array<float, 256> angularAcc_x;
    alignas(32) std::array<float, 256> angularAcc_y;
    alignas(32) std::array<float, 256> angularAcc_z;
    
    // Previous angular acceleration (for Verlet integration)
    alignas(32) std::array<float, 256> prevAngularAcc_x;
    alignas(32) std::array<float, 256> prevAngularAcc_y;
    alignas(32) std::array<float, 256> prevAngularAcc_z;
    
    // Physics properties
    alignas(32) std::array<float, 256> mass;
    alignas(32) std::array<float, 256> radius;
    alignas(32) std::array<float, 256> age;
    alignas(32) std::array<float, 256> energy;
    
    // Cell state
    alignas(32) std::array<uint32_t, 256> cellType;
    alignas(32) std::array<uint32_t, 256> genomeID;
    alignas(32) std::array<uint32_t, 256> flags;
    
    // Visual properties
    alignas(32) std::array<float, 256> color_r;
    alignas(32) std::array<float, 256> color_g;
    alignas(32) std::array<float, 256> color_b;
    
    // Adhesion index management (20 slots per cell, -1 for empty)
    alignas(32) std::array<std::array<int, 20>, 256> adhesionIndices;
    
    size_t activeCellCount = 0;
};

// CPU SoA structure for adhesion connections (GPU-Compatible)
// Matches GPU AdhesionConnection structure exactly with SoA layout for SIMD processing
// Capacity: 5,120 connections (20 × 256 cells) as per requirements 7.1, 7.2, 7.3
struct CPUAdhesionConnections_SoA {
    // Connection topology (5,120 capacity = 256 cells × 20 connections)
    alignas(32) std::array<uint32_t, 5120> cellAIndex;
    alignas(32) std::array<uint32_t, 5120> cellBIndex;
    alignas(32) std::array<uint32_t, 5120> modeIndex;
    alignas(32) std::array<uint32_t, 5120> isActive;
    
    // Zone classification for division inheritance
    alignas(32) std::array<uint32_t, 5120> zoneA;
    alignas(32) std::array<uint32_t, 5120> zoneB;
    
    // Anchor directions in local cell space (normalized)
    alignas(32) std::array<float, 5120> anchorDirectionA_x;
    alignas(32) std::array<float, 5120> anchorDirectionA_y;
    alignas(32) std::array<float, 5120> anchorDirectionA_z;
    alignas(32) std::array<float, 5120> anchorDirectionB_x;
    alignas(32) std::array<float, 5120> anchorDirectionB_y;
    alignas(32) std::array<float, 5120> anchorDirectionB_z;
    
    // Twist constraint reference quaternions
    alignas(32) std::array<float, 5120> twistReferenceA_x;
    alignas(32) std::array<float, 5120> twistReferenceA_y;
    alignas(32) std::array<float, 5120> twistReferenceA_z;
    alignas(32) std::array<float, 5120> twistReferenceA_w;
    alignas(32) std::array<float, 5120> twistReferenceB_x;
    alignas(32) std::array<float, 5120> twistReferenceB_y;
    alignas(32) std::array<float, 5120> twistReferenceB_z;
    alignas(32) std::array<float, 5120> twistReferenceB_w;
    
    size_t activeConnectionCount = 0;
};

// Validation for structure size and alignment (Requirements 7.1, 7.2, 7.3)
static_assert(alignof(CPUAdhesionConnections_SoA) >= 32, "CPUAdhesionConnections_SoA must have 32-byte alignment for SIMD operations");
static_assert(sizeof(CPUAdhesionConnections_SoA) % 32 == 0, "CPUAdhesionConnections_SoA must be 32-byte aligned with zero padding waste");

// Validate capacity matches requirements (5,120 connections = 20 × 256 cells)
static_assert(std::tuple_size_v<decltype(CPUAdhesionConnections_SoA::cellAIndex)> == 5120, "Connection capacity must be 5,120 (20 × 256 cells)");

// Validate SIMD compatibility (capacity must be multiple of 8 for AVX2)
static_assert(5120 % 8 == 0, "Connection capacity must be multiple of 8 for SIMD operations");

// CPU Genome parameters for instant updates
struct CPUGenomeParameters {
    // Adhesion settings (actual settings from genome, not derived strength)
    AdhesionSettings adhesionSettings;
    
    float divisionThreshold;
    float metabolicRate;
    float mutationRate;
    glm::vec3 preferredDirection;
    glm::vec3 modeColor;  // RGB color for this mode

    // Cell division parameters (matching GPU implementation)
    glm::vec3 splitDirection;  // Direction vector for cell division
    glm::quat childOrientationA; // Orientation quaternion for child A
    glm::quat childOrientationB; // Orientation quaternion for child B
    uint32_t cellTypeFlags;
    uint32_t childModeA;       // Mode index for child A
    uint32_t childModeB;       // Mode index for child B
    uint32_t divisionFlags;    // Bit flags: bit 0 = childAKeepAdhesion, bit 1 = childBKeepAdhesion
};

// CPU Cell creation parameters
struct CPUCellParameters {
    glm::vec3 position;
    glm::vec3 velocity;
    glm::quat orientation;
    float mass;
    float radius;
    uint32_t cellType;
    uint32_t genomeID;
    CPUGenomeParameters genome;
};

// CPU Adhesion connection parameters
struct CPUAdhesionParameters {
    glm::vec3 anchorDirectionA;  // Anchor direction for cell A
    glm::vec3 anchorDirectionB;  // Anchor direction for cell B
    uint32_t modeIndex;          // Mode index for adhesion settings
    uint32_t zoneA;              // Zone classification for cell A
    uint32_t zoneB;              // Zone classification for cell B
    glm::quat twistReferenceA;   // Twist reference quaternion for cell A
    glm::quat twistReferenceB;   // Twist reference quaternion for cell B
};

/**
 * CPU Native SoA Data Manager
 * 
 * Manages Structure-of-Arrays data layout for optimal CPU performance.
 * Eliminates conversion overhead by using SoA as the primary format.
 * Provides native .soa file format independent of existing .aos format.
 * 
 * Requirements addressed: 3.1, 3.2, 3.5
 */
class CPUSoADataManager {
public:
    CPUSoADataManager();
    ~CPUSoADataManager();

    // Scene management (native CPU SoA format)
    void createEmptyScene(size_t maxCells);
    void savePreviewScene(const std::string& filename);
    void loadPreviewScene(const std::string& filename);

    // Native CPU SoA operations (no conversion overhead)
    uint32_t addCell(const CPUCellParameters& params);
    void removeCell(uint32_t cellIndex);
    void addAdhesionConnection(uint32_t cellA, uint32_t cellB, const CPUAdhesionParameters& params);

    // Direct parameter updates (instant genome iteration)
    void updateGenomeParameters(uint32_t cellIndex, const CPUGenomeParameters& params);
    void updateCellPosition(uint32_t cellIndex, const glm::vec3& position);
    void updateCellVelocity(uint32_t cellIndex, const glm::vec3& velocity);

    // Data access for CPU physics engine
    CPUCellPhysics_SoA& getCellData() { return m_cellData; }
    const CPUCellPhysics_SoA& getCellData() const { return m_cellData; }
    CPUAdhesionConnections_SoA& getAdhesionData() { return m_adhesionData; }
    const CPUAdhesionConnections_SoA& getAdhesionData() const { return m_adhesionData; }
    
    // Connection management access
    class CPUAdhesionConnectionManager* getConnectionManager() { return m_connectionManager.get(); }

    // System information
    size_t getActiveCellCount() const { return m_cellData.activeCellCount; }
    size_t getActiveConnectionCount() const { return m_adhesionData.activeConnectionCount; }
    size_t getMaxCells() const { return 256; }

    // Data integrity and validation
    void validateDataIntegrity();
    void validateSoAStructures();
    void analyzePaddingEfficiency();
    void runValidationTests(); // Run comprehensive validation tests
    void compactArrays(); // Remove gaps from deleted cells
    
    // Connection management validation (Requirements 10.4, 7.4, 7.5)
    void validateConnectionIntegrity();
    void runConnectionManagerTests();
    
    // Structure validation for enhanced adhesion support
    static void validateAdhesionStructureAlignment();
    static void analyzeAdhesionMemoryLayout();
    void testEnhancedAdhesionStructure(); // Test the enhanced structure

private:
    // Native CPU SoA data storage
    CPUCellPhysics_SoA m_cellData;
    CPUAdhesionConnections_SoA m_adhesionData;
    
    // Free index management
    std::vector<uint32_t> m_freeCellIndices;
    std::vector<uint32_t> m_freeConnectionIndices;
    
    // Connection management system (Requirements 10.1-10.5, 7.4, 7.5)
    std::unique_ptr<class CPUAdhesionConnectionManager> m_connectionManager;

    // Internal methods
    uint32_t allocateCellIndex();
    uint32_t allocateConnectionIndex();
    void deallocateCellIndex(uint32_t index);
    void deallocateConnectionIndex(uint32_t index);
    
    // File format methods
    void serializeSoAData(const std::string& filename);
    void deserializeSoAData(const std::string& filename);
};