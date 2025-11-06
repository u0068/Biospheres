#pragma once

#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include "../cell/common_structs.h"

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
    
    // Orientation (quaternion)
    alignas(32) std::array<float, 256> quat_x;
    alignas(32) std::array<float, 256> quat_y;
    alignas(32) std::array<float, 256> quat_z;
    alignas(32) std::array<float, 256> quat_w;
    
    // Physics properties
    alignas(32) std::array<float, 256> mass;
    alignas(32) std::array<float, 256> radius;
    alignas(32) std::array<float, 256> age;
    alignas(32) std::array<float, 256> energy;
    
    // Cell state
    alignas(32) std::array<uint32_t, 256> cellType;
    alignas(32) std::array<uint32_t, 256> genomeID;
    alignas(32) std::array<uint32_t, 256> flags;
    
    size_t activeCellCount = 0;
};

// CPU SoA structure for adhesion connections
struct CPUAdhesionConnections_SoA {
    alignas(32) std::array<uint32_t, 1024> cellA_indices;
    alignas(32) std::array<uint32_t, 1024> cellB_indices;
    alignas(32) std::array<float, 1024> anchor_dir_x;
    alignas(32) std::array<float, 1024> anchor_dir_y;
    alignas(32) std::array<float, 1024> anchor_dir_z;
    alignas(32) std::array<float, 1024> rest_length;
    alignas(32) std::array<float, 1024> stiffness;
    alignas(32) std::array<float, 1024> twist_constraint;
    
    size_t activeConnectionCount = 0;
};

// CPU Genome parameters for instant updates
struct CPUGenomeParameters {
    float adhesionStrength;
    float divisionThreshold;
    float metabolicRate;
    float mutationRate;
    glm::vec3 preferredDirection;
    uint32_t cellTypeFlags;
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
    glm::vec3 anchorDirection;
    float restLength;
    float stiffness;
    float twistConstraint;
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

private:
    // Native CPU SoA data storage
    CPUCellPhysics_SoA m_cellData;
    CPUAdhesionConnections_SoA m_adhesionData;
    
    // Free index management
    std::vector<uint32_t> m_freeCellIndices;
    std::vector<uint32_t> m_freeConnectionIndices;

    // Internal methods
    uint32_t allocateCellIndex();
    uint32_t allocateConnectionIndex();
    void deallocateCellIndex(uint32_t index);
    void deallocateConnectionIndex(uint32_t index);
    
    // File format methods
    void serializeSoAData(const std::string& filename);
    void deserializeSoAData(const std::string& filename);
};