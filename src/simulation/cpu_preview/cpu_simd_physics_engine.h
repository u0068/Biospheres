#pragma once

#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <cmath>
#include <immintrin.h> // AVX2 SIMD intrinsics
#include "cpu_soa_data_manager.h"

/**
 * CPU SIMD-Optimized Physics Engine
 * 
 * Implements behavioral equivalence with GPU compute shaders using CPU SIMD operations.
 * Uses AVX2 instructions for vectorized processing of 8 floats simultaneously.
 * Maintains identical physics algorithms within documented tolerances.
 * 
 * Requirements addressed: 4.2, 4.4, 2.1, 2.2, 2.3, 2.4, 2.5
 */
class CPUSIMDPhysicsEngine {
public:
    CPUSIMDPhysicsEngine();
    ~CPUSIMDPhysicsEngine();

    // Main simulation step
    void simulateStep(CPUCellPhysics_SoA& cells, 
                     CPUAdhesionConnections_SoA& adhesions,
                     float deltaTime);

    // Performance monitoring
    float getLastStepTime() const { return m_lastStepTime; }
    size_t getProcessedCellCount() const { return m_processedCellCount; }

    // Behavioral equivalence validation
    struct ValidationMetrics {
        float maxPositionError = 0.0f;
        float maxVelocityError = 0.0f;
        float adhesionEnergyDifference = 0.0f;
        bool withinTolerance = true;
    };
    
    ValidationMetrics getLastValidationMetrics() const { return m_lastValidation; }

private:
    // CPU SIMD-optimized physics operations
    void calculateCollisionForces(CPUCellPhysics_SoA& cells);
    void calculateAdhesionForces(CPUCellPhysics_SoA& cells, 
                                const CPUAdhesionConnections_SoA& adhesions);
    void integrateVerlet(CPUCellPhysics_SoA& cells, float deltaTime);
    void updateOrientations(CPUCellPhysics_SoA& cells, float deltaTime);

    // Spatial optimization for CPU performance
    void updateSpatialGrid(const CPUCellPhysics_SoA& cells);
    std::vector<uint32_t> getNeighbors(uint32_t cellIndex, float radius);
    void applyAccelerationDamping(CPUCellPhysics_SoA& cells);

    // CPU SIMD utility functions (AVX2 optimized)
    void simd_vec3_add(float* a_x, float* a_y, float* a_z,
                       const float* b_x, const float* b_y, const float* b_z,
                       size_t count);
    void simd_vec3_scale(float* vec_x, float* vec_y, float* vec_z,
                        float scalar, size_t count);
    void simd_vec3_normalize(float* vec_x, float* vec_y, float* vec_z,
                            size_t count);
    void simd_distance_squared(const float* pos1_x, const float* pos1_y, const float* pos1_z,
                              const float* pos2_x, const float* pos2_y, const float* pos2_z,
                              float* result, size_t count);

    // Collision detection (behavioral equivalence with GPU)
    void processCollisionPair(uint32_t cellA, uint32_t cellB, 
                             CPUCellPhysics_SoA& cells);
    void processBatchCollisions(CPUCellPhysics_SoA& cells);
    void simd_collision_detection_batch(const float* pos_x, const float* pos_y, const float* pos_z,
                                       const float* radius, float* acc_x, float* acc_y, float* acc_z,
                                       const float* mass, size_t start_idx, size_t count);
    bool sphereCollisionTest(const glm::vec3& posA, float radiusA,
                            const glm::vec3& posB, float radiusB,
                            float& penetrationDepth, glm::vec3& normal);

    // Adhesion physics (identical to GPU implementation)
    void processAdhesionConnection(uint32_t connectionIndex,
                                  const CPUAdhesionConnections_SoA& adhesions,
                                  CPUCellPhysics_SoA& cells);
    void simd_adhesion_force_batch(CPUCellPhysics_SoA& cells,
                                  const CPUAdhesionConnections_SoA& adhesions,
                                  size_t start_idx, size_t count);
    float calculateAdhesionEnergy(const CPUAdhesionConnections_SoA& adhesions,
                                 const CPUCellPhysics_SoA& cells);
    
    // Cache-friendly batch processing
    void processCellBatch(CPUCellPhysics_SoA& cells, size_t startIdx, size_t count);
    void prefetchCellData(const CPUCellPhysics_SoA& cells, size_t startIdx, size_t count);

    // Verlet integration (matching GPU precision)
    void verletIntegrationStep(CPUCellPhysics_SoA& cells, float deltaTime);
    void updateVelocities(CPUCellPhysics_SoA& cells, float deltaTime);
    void applyBoundaryConstraints(CPUCellPhysics_SoA& cells);

    // Spatial grid system (CPU cache optimized)
    struct CPUSpatialGrid {
        static constexpr int GRID_SIZE = 64;
        static constexpr float CELL_SIZE = 100.0f / GRID_SIZE; // config::WORLD_SIZE / GRID_SIZE
        static constexpr int MAX_CELLS_PER_GRID = 32; // CPU cache optimization
        static constexpr int TOTAL_GRID_CELLS = GRID_SIZE * GRID_SIZE * GRID_SIZE;
        
        // CPU-optimized data layout for cache efficiency
        struct GridCell {
            alignas(32) std::array<uint32_t, MAX_CELLS_PER_GRID> cellIndices;
            uint32_t count = 0;
            uint32_t _padding[7]; // Align to 32-byte boundary for SIMD
        };
        
        std::vector<GridCell> gridCells;
        
        // Preallocated buffers to avoid dynamic allocation
        std::vector<uint32_t> queryResultBuffer;
        std::vector<glm::ivec3> neighborCoords;
        
        void initialize();
        void clear();
        void insert(uint32_t cellIndex, const glm::vec3& position);
        std::vector<uint32_t> query(const glm::vec3& position, float radius);
        void queryIntoBuffer(const glm::vec3& position, float radius, std::vector<uint32_t>& results);
        
        // Cache-friendly neighbor iteration
        template<typename Callback>
        void iterateNeighbors(const glm::vec3& position, float radius, Callback callback) {
            // Cache-friendly iteration without temporary allocations
            glm::ivec3 centerCoord = getGridCoord(position);
            int searchRadius = static_cast<int>(std::ceil(radius / CELL_SIZE));
            
            for (const auto& offset : neighborOffsets) {
                glm::ivec3 coord = centerCoord + offset * searchRadius;
                
                if (isValidCoord(coord)) {
                    uint32_t gridIndex = getGridIndex(coord);
                    const GridCell& cell = gridCells[gridIndex];
                    
                    // Process cells in this grid cell
                    for (uint32_t i = 0; i < cell.count; ++i) {
                        callback(cell.cellIndices[i]);
                    }
                }
            }
        }
        
    private:
        glm::ivec3 getGridCoord(const glm::vec3& position);
        bool isValidCoord(const glm::ivec3& coord);
        uint32_t getGridIndex(const glm::ivec3& coord);
        void precomputeNeighborOffsets(int searchRadius);
        
        // Precomputed neighbor offsets for cache efficiency
        std::vector<glm::ivec3> neighborOffsets;
    };

    // Performance and validation tracking
    float m_lastStepTime = 0.0f;
    size_t m_processedCellCount = 0;
    ValidationMetrics m_lastValidation;
    
    // Spatial optimization
    std::unique_ptr<CPUSpatialGrid> m_spatialGrid;
    
    // Preallocated buffers to avoid dynamic allocation during simulation
    struct PreallocatedBuffers {
        std::vector<uint32_t> neighborBuffer;
        std::vector<glm::vec3> forceBuffer;
        std::vector<float> distanceBuffer;
        alignas(32) std::array<float, 256> tempPositions_x;
        alignas(32) std::array<float, 256> tempPositions_y;
        alignas(32) std::array<float, 256> tempPositions_z;
        
        void initialize() {
            neighborBuffer.reserve(1024);
            forceBuffer.reserve(256);
            distanceBuffer.reserve(256);
        }
    };
    
    PreallocatedBuffers m_buffers;
    
    // Physics constants (matching GPU implementation)
    static constexpr float COLLISION_DAMPING = 0.8f;
    static constexpr float ADHESION_STIFFNESS = 100.0f;
    static constexpr float POSITION_TOLERANCE = 1e-3f;
    static constexpr float ADHESION_ENERGY_TOLERANCE = 0.005f; // 0.5%
    
    // SIMD processing constants
    static constexpr size_t SIMD_WIDTH = 8; // AVX2 float count
    static constexpr size_t BLOCK_SIZE = 32; // Cache line optimization
    
    // Internal timing
    std::chrono::steady_clock::time_point m_stepStart;
};