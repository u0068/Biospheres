#include "cpu_soa_validation.h"
#include <cmath>
#include <iomanip>

namespace CPUSoAValidation {

    // Implementation of validation functions that require runtime execution
    
    void runComprehensiveValidation(const CPUCellPhysics_SoA& cellData, 
                                   const CPUAdhesionConnections_SoA& adhesionData) {
        try {
            // Structure validation
            SoAStructureValidator::validateCellPhysicsStructure(cellData);
            SoAStructureValidator::validateAdhesionConnectionsStructure(adhesionData);
            
            // Data integrity validation
            SoADataIntegrityValidator::validateCellDataIntegrity(cellData);
            SoADataIntegrityValidator::validateAdhesionDataIntegrity(adhesionData, cellData);
            
            // Bounds checking
            SoADataIntegrityValidator::validateBounds(cellData, adhesionData);
            
            std::cout << "✓ All SoA validation checks passed successfully\n";
        }
        catch (const std::exception& e) {
            std::cerr << "❌ SoA validation failed: " << e.what() << std::endl;
            throw;
        }
    }

    void printDetailedStructureInfo() {
        std::cout << "=== CPU SoA Structure Information ===\n\n";
        
        // CPUCellPhysics_SoA analysis
        std::cout << "CPUCellPhysics_SoA Structure:\n";
        std::cout << "  Total size: " << sizeof(CPUCellPhysics_SoA) << " bytes\n";
        std::cout << "  Alignment: " << alignof(CPUCellPhysics_SoA) << " bytes\n";
        std::cout << "  Array count: 21 arrays (pos_xyz, vel_xyz, acc_xyz, quat_xyzw, mass, radius, age, energy, cellType, genomeID, flags)\n";
        std::cout << "  Elements per array: " << MAX_CELLS << "\n";
        std::cout << "  Float arrays: 17 (pos_xyz, vel_xyz, acc_xyz, quat_xyzw, mass, radius, age, energy)\n";
        std::cout << "  Uint32 arrays: 3 (cellType, genomeID, flags)\n";
        std::cout << "  Memory per float array: " << (MAX_CELLS * sizeof(float)) << " bytes\n";
        std::cout << "  Memory per uint32 array: " << (MAX_CELLS * sizeof(uint32_t)) << " bytes\n";
        
        size_t totalArrayMemory = (17 * MAX_CELLS * sizeof(float)) + (3 * MAX_CELLS * sizeof(uint32_t));
        std::cout << "  Total array memory: " << totalArrayMemory << " bytes\n";
        std::cout << "  Overhead: " << (sizeof(CPUCellPhysics_SoA) - totalArrayMemory) << " bytes\n\n";
        
        // CPUAdhesionConnections_SoA analysis
        std::cout << "CPUAdhesionConnections_SoA Structure:\n";
        std::cout << "  Total size: " << sizeof(CPUAdhesionConnections_SoA) << " bytes\n";
        std::cout << "  Alignment: " << alignof(CPUAdhesionConnections_SoA) << " bytes\n";
        std::cout << "  Array count: 8 arrays (cellA_indices, cellB_indices, anchor_dir_xyz, rest_length, stiffness, twist_constraint)\n";
        std::cout << "  Elements per array: " << MAX_CONNECTIONS << "\n";
        std::cout << "  Uint32 arrays: 2 (cellA_indices, cellB_indices)\n";
        std::cout << "  Float arrays: 6 (anchor_dir_xyz, rest_length, stiffness, twist_constraint)\n";
        std::cout << "  Memory per uint32 array: " << (MAX_CONNECTIONS * sizeof(uint32_t)) << " bytes\n";
        std::cout << "  Memory per float array: " << (MAX_CONNECTIONS * sizeof(float)) << " bytes\n";
        
        size_t totalAdhesionMemory = (2 * MAX_CONNECTIONS * sizeof(uint32_t)) + (6 * MAX_CONNECTIONS * sizeof(float));
        std::cout << "  Total array memory: " << totalAdhesionMemory << " bytes\n";
        std::cout << "  Overhead: " << (sizeof(CPUAdhesionConnections_SoA) - totalAdhesionMemory) << " bytes\n\n";
        
        // Parameter structures analysis
        std::cout << "Parameter Structures:\n";
        std::cout << "  CPUGenomeParameters: " << sizeof(CPUGenomeParameters) << " bytes\n";
        std::cout << "  CPUCellParameters: " << sizeof(CPUCellParameters) << " bytes\n";
        std::cout << "  CPUAdhesionParameters: " << sizeof(CPUAdhesionParameters) << " bytes\n\n";
        
        // Memory efficiency summary
        float cellEfficiency = (static_cast<float>(totalArrayMemory) / sizeof(CPUCellPhysics_SoA)) * 100.0f;
        float adhesionEfficiency = (static_cast<float>(totalAdhesionMemory) / sizeof(CPUAdhesionConnections_SoA)) * 100.0f;
        
        std::cout << "Memory Efficiency:\n";
        std::cout << "  CPUCellPhysics_SoA: " << std::fixed << std::setprecision(2) << cellEfficiency << "%\n";
        std::cout << "  CPUAdhesionConnections_SoA: " << std::fixed << std::setprecision(2) << adhesionEfficiency << "%\n\n";
        
        // SIMD optimization info
        std::cout << "SIMD Optimization:\n";
        std::cout << "  Alignment requirement: " << SIMD_ALIGNMENT << " bytes (AVX2)\n";
        std::cout << "  Elements per SIMD operation: 8 floats or 8 uint32s\n";
        std::cout << "  Optimal processing block size: " << (SIMD_ALIGNMENT / sizeof(float)) << " elements\n";
        std::cout << "  Total SIMD blocks per array: " << (MAX_CELLS / 8) << "\n";
    }

    void validateMemoryLayout() {
        std::cout << "=== Memory Layout Validation ===\n";
        
        // Create test instances to check actual memory layout
        CPUCellPhysics_SoA testCellData{};
        CPUAdhesionConnections_SoA testAdhesionData{};
        
        // Check alignment of first elements in each array
        std::cout << "Array alignment validation:\n";
        
        // Cell physics arrays
        uintptr_t pos_x_addr = reinterpret_cast<uintptr_t>(testCellData.pos_x.data());
        uintptr_t vel_x_addr = reinterpret_cast<uintptr_t>(testCellData.vel_x.data());
        uintptr_t mass_addr = reinterpret_cast<uintptr_t>(testCellData.mass.data());
        
        std::cout << "  pos_x alignment: " << (pos_x_addr % SIMD_ALIGNMENT) << " (should be 0)\n";
        std::cout << "  vel_x alignment: " << (vel_x_addr % SIMD_ALIGNMENT) << " (should be 0)\n";
        std::cout << "  mass alignment: " << (mass_addr % SIMD_ALIGNMENT) << " (should be 0)\n";
        
        // Adhesion connection arrays
        uintptr_t cellA_addr = reinterpret_cast<uintptr_t>(testAdhesionData.cellA_indices.data());
        uintptr_t anchor_x_addr = reinterpret_cast<uintptr_t>(testAdhesionData.anchor_dir_x.data());
        
        std::cout << "  cellA_indices alignment: " << (cellA_addr % SIMD_ALIGNMENT) << " (should be 0)\n";
        std::cout << "  anchor_dir_x alignment: " << (anchor_x_addr % SIMD_ALIGNMENT) << " (should be 0)\n";
        
        // Validate all alignments are correct
        bool allAligned = (pos_x_addr % SIMD_ALIGNMENT == 0) &&
                         (vel_x_addr % SIMD_ALIGNMENT == 0) &&
                         (mass_addr % SIMD_ALIGNMENT == 0) &&
                         (cellA_addr % SIMD_ALIGNMENT == 0) &&
                         (anchor_x_addr % SIMD_ALIGNMENT == 0);
        
        if (allAligned) {
            std::cout << "✓ All arrays are properly aligned for SIMD operations\n";
        } else {
            std::cout << "❌ Some arrays are not properly aligned for SIMD operations\n";
        }
        
        // Check cache line alignment
        std::cout << "\nCache line analysis:\n";
        std::cout << "  Cache line size: " << CACHE_LINE_SIZE << " bytes\n";
        std::cout << "  Elements per cache line (float): " << (CACHE_LINE_SIZE / sizeof(float)) << "\n";
        std::cout << "  Elements per cache line (uint32): " << (CACHE_LINE_SIZE / sizeof(uint32_t)) << "\n";
        
        // Calculate how many cache lines each array spans
        size_t float_array_size = MAX_CELLS * sizeof(float);
        size_t uint32_array_size = MAX_CELLS * sizeof(uint32_t);
        size_t float_cache_lines = (float_array_size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
        size_t uint32_cache_lines = (uint32_array_size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
        
        std::cout << "  Float arrays span: " << float_cache_lines << " cache lines each\n";
        std::cout << "  Uint32 arrays span: " << uint32_cache_lines << " cache lines each\n";
    }

    void performanceAnalysis() {
        std::cout << "=== Performance Analysis ===\n";
        
        // Calculate theoretical memory bandwidth requirements
        size_t cellDataSize = sizeof(CPUCellPhysics_SoA);
        size_t adhesionDataSize = sizeof(CPUAdhesionConnections_SoA);
        size_t totalMemory = cellDataSize + adhesionDataSize;
        
        std::cout << "Memory footprint:\n";
        std::cout << "  Cell data: " << (cellDataSize / 1024) << " KB\n";
        std::cout << "  Adhesion data: " << (adhesionDataSize / 1024) << " KB\n";
        std::cout << "  Total: " << (totalMemory / 1024) << " KB\n";
        
        // Estimate cache behavior
        const size_t L1_CACHE_SIZE = 32 * 1024;  // Typical L1 cache size
        const size_t L2_CACHE_SIZE = 256 * 1024; // Typical L2 cache size
        const size_t L3_CACHE_SIZE = 8 * 1024 * 1024; // Typical L3 cache size
        
        std::cout << "\nCache analysis:\n";
        std::cout << "  Fits in L1 cache: " << (totalMemory <= L1_CACHE_SIZE ? "Yes" : "No") << "\n";
        std::cout << "  Fits in L2 cache: " << (totalMemory <= L2_CACHE_SIZE ? "Yes" : "No") << "\n";
        std::cout << "  Fits in L3 cache: " << (totalMemory <= L3_CACHE_SIZE ? "Yes" : "No") << "\n";
        
        // SIMD operation estimates
        size_t float_elements = 17 * MAX_CELLS; // 17 float arrays
        size_t uint32_elements = 5 * MAX_CELLS; // 3 cell arrays + 2 adhesion arrays
        size_t simd_float_ops = float_elements / 8; // 8 floats per AVX2 operation
        size_t simd_uint32_ops = uint32_elements / 8; // 8 uint32s per AVX2 operation
        
        std::cout << "\nSIMD operation estimates:\n";
        std::cout << "  Float SIMD operations: " << simd_float_ops << "\n";
        std::cout << "  Uint32 SIMD operations: " << simd_uint32_ops << "\n";
        std::cout << "  Total SIMD operations: " << (simd_float_ops + simd_uint32_ops) << "\n";
        
        // Bandwidth estimates (assuming typical memory speeds)
        const float DDR4_BANDWIDTH = 25.6f; // GB/s for DDR4-3200
        const float L3_BANDWIDTH = 100.0f;  // GB/s typical L3 bandwidth
        
        float memory_transfer_time = (totalMemory / (1024.0f * 1024.0f * 1024.0f)) / DDR4_BANDWIDTH * 1000.0f; // ms
        float cache_transfer_time = (totalMemory / (1024.0f * 1024.0f * 1024.0f)) / L3_BANDWIDTH * 1000.0f; // ms
        
        std::cout << "\nBandwidth estimates:\n";
        std::cout << "  Memory transfer time: " << std::fixed << std::setprecision(3) << memory_transfer_time << " ms\n";
        std::cout << "  Cache transfer time: " << std::fixed << std::setprecision(3) << cache_transfer_time << " ms\n";
        std::cout << "  Target simulation time: 16.0 ms\n";
        
        if (cache_transfer_time < 1.0f) {
            std::cout << "✓ Memory bandwidth should not be a bottleneck\n";
        } else {
            std::cout << "⚠ Memory bandwidth may impact performance\n";
        }
    }

    void validateBoundsChecking(const CPUCellPhysics_SoA& cellData, 
                               const CPUAdhesionConnections_SoA& adhesionData) {
        std::cout << "=== Bounds Checking Validation ===\n";
        
        // Test array bounds
        if (cellData.activeCellCount > MAX_CELLS) {
            throw std::runtime_error("Cell count exceeds maximum bounds");
        }
        
        if (adhesionData.activeConnectionCount > MAX_CONNECTIONS) {
            throw std::runtime_error("Connection count exceeds maximum bounds");
        }
        
        // Validate all active cells have reasonable values
        for (size_t i = 0; i < cellData.activeCellCount; ++i) {
            // Check position bounds (reasonable world coordinates)
            const float MAX_WORLD_COORD = 1000.0f;
            if (std::abs(cellData.pos_x[i]) > MAX_WORLD_COORD ||
                std::abs(cellData.pos_y[i]) > MAX_WORLD_COORD ||
                std::abs(cellData.pos_z[i]) > MAX_WORLD_COORD) {
                throw std::runtime_error("Cell position out of reasonable bounds at index " + std::to_string(i));
            }
            
            // Check velocity bounds (reasonable physics)
            const float MAX_VELOCITY = 100.0f;
            if (std::abs(cellData.vel_x[i]) > MAX_VELOCITY ||
                std::abs(cellData.vel_y[i]) > MAX_VELOCITY ||
                std::abs(cellData.vel_z[i]) > MAX_VELOCITY) {
                throw std::runtime_error("Cell velocity out of reasonable bounds at index " + std::to_string(i));
            }
            
            // Check mass and radius bounds
            const float MAX_MASS = 1000.0f;
            const float MAX_RADIUS = 10.0f;
            if (cellData.mass[i] > MAX_MASS || cellData.radius[i] > MAX_RADIUS) {
                throw std::runtime_error("Cell physical properties out of bounds at index " + std::to_string(i));
            }
        }
        
        std::cout << "✓ Bounds checking validation passed\n";
    }

    void validateNumericalStability(const CPUCellPhysics_SoA& cellData) {
        std::cout << "=== Numerical Stability Validation ===\n";
        
        for (size_t i = 0; i < cellData.activeCellCount; ++i) {
            // Check for NaN values
            if (std::isnan(cellData.pos_x[i]) || std::isnan(cellData.pos_y[i]) || std::isnan(cellData.pos_z[i])) {
                throw std::runtime_error("NaN detected in position at cell index " + std::to_string(i));
            }
            
            if (std::isnan(cellData.vel_x[i]) || std::isnan(cellData.vel_y[i]) || std::isnan(cellData.vel_z[i])) {
                throw std::runtime_error("NaN detected in velocity at cell index " + std::to_string(i));
            }
            
            if (std::isnan(cellData.mass[i]) || std::isnan(cellData.radius[i]) || 
                std::isnan(cellData.age[i]) || std::isnan(cellData.energy[i])) {
                throw std::runtime_error("NaN detected in physical properties at cell index " + std::to_string(i));
            }
            
            // Check for infinite values
            if (std::isinf(cellData.pos_x[i]) || std::isinf(cellData.pos_y[i]) || std::isinf(cellData.pos_z[i])) {
                throw std::runtime_error("Infinite value detected in position at cell index " + std::to_string(i));
            }
            
            if (std::isinf(cellData.vel_x[i]) || std::isinf(cellData.vel_y[i]) || std::isinf(cellData.vel_z[i])) {
                throw std::runtime_error("Infinite value detected in velocity at cell index " + std::to_string(i));
            }
            
            // Check quaternion normalization (numerical stability)
            float quat_length_sq = cellData.quat_x[i] * cellData.quat_x[i] + 
                                  cellData.quat_y[i] * cellData.quat_y[i] + 
                                  cellData.quat_z[i] * cellData.quat_z[i] + 
                                  cellData.quat_w[i] * cellData.quat_w[i];
            
            if (quat_length_sq < 0.99f || quat_length_sq > 1.01f) {
                throw std::runtime_error("Quaternion normalization error at cell index " + std::to_string(i) + 
                                       " (length_sq = " + std::to_string(quat_length_sq) + ")");
            }
        }
        
        std::cout << "✓ Numerical stability validation passed\n";
    }

    void validateSIMDCompatibility() {
        std::cout << "=== SIMD Compatibility Validation ===\n";
        
        // Check that array sizes are compatible with SIMD operations
        if (MAX_CELLS % 8 != 0) {
            std::cout << "⚠ Warning: MAX_CELLS (" << MAX_CELLS << ") is not a multiple of 8, may not be optimal for AVX2\n";
        } else {
            std::cout << "✓ MAX_CELLS is SIMD-compatible (multiple of 8)\n";
        }
        
        if (MAX_CONNECTIONS % 8 != 0) {
            std::cout << "⚠ Warning: MAX_CONNECTIONS (" << MAX_CONNECTIONS << ") is not a multiple of 8, may not be optimal for AVX2\n";
        } else {
            std::cout << "✓ MAX_CONNECTIONS is SIMD-compatible (multiple of 8)\n";
        }
        
        // Check alignment requirements
        std::cout << "Alignment requirements:\n";
        std::cout << "  Required SIMD alignment: " << SIMD_ALIGNMENT << " bytes\n";
        std::cout << "  CPUCellPhysics_SoA alignment: " << alignof(CPUCellPhysics_SoA) << " bytes\n";
        std::cout << "  CPUAdhesionConnections_SoA alignment: " << alignof(CPUAdhesionConnections_SoA) << " bytes\n";
        
        if (alignof(CPUCellPhysics_SoA) >= SIMD_ALIGNMENT && 
            alignof(CPUAdhesionConnections_SoA) >= SIMD_ALIGNMENT) {
            std::cout << "✓ All structures meet SIMD alignment requirements\n";
        } else {
            throw std::runtime_error("Structures do not meet SIMD alignment requirements");
        }
        
        // Calculate SIMD operation efficiency
        size_t total_float_elements = 17 * MAX_CELLS + 6 * MAX_CONNECTIONS; // Float arrays
        size_t total_uint32_elements = 3 * MAX_CELLS + 2 * MAX_CONNECTIONS; // Uint32 arrays
        size_t simd_float_ops = total_float_elements / 8;
        size_t simd_uint32_ops = total_uint32_elements / 8;
        
        std::cout << "SIMD operation efficiency:\n";
        std::cout << "  Total float elements: " << total_float_elements << "\n";
        std::cout << "  SIMD float operations: " << simd_float_ops << "\n";
        std::cout << "  Float SIMD efficiency: " << ((simd_float_ops * 8.0f) / total_float_elements * 100.0f) << "%\n";
        std::cout << "  Total uint32 elements: " << total_uint32_elements << "\n";
        std::cout << "  SIMD uint32 operations: " << simd_uint32_ops << "\n";
        std::cout << "  Uint32 SIMD efficiency: " << ((simd_uint32_ops * 8.0f) / total_uint32_elements * 100.0f) << "%\n";
    }

} // namespace CPUSoAValidation