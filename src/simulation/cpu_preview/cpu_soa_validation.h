#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <stdexcept>
#include <iostream>
#include <type_traits>
#include "cpu_soa_data_manager.h"

/**
 * CPU SoA Data Structure Validation and Integrity Checks
 * 
 * Provides comprehensive validation for Structure-of-Arrays data layout.
 * Ensures optimal field ordering, SIMD alignment, and zero padding waste.
 * Validates data integrity and bounds checking for CPU Preview System.
 * 
 * Requirements addressed: 4.1, 4.4
 */

namespace CPUSoAValidation {

    // Alignment validation constants
    static constexpr size_t SIMD_ALIGNMENT = 32;  // AVX2 alignment requirement
    static constexpr size_t CACHE_LINE_SIZE = 64; // CPU cache line size
    static constexpr size_t MAX_CELLS = 256;
    static constexpr size_t MAX_CONNECTIONS = 5120;

    /**
     * Comprehensive padding analysis for SoA structures
     * Validates zero padding waste and optimal field ordering
     */
    template<typename T>
    struct PaddingAnalysis {
        size_t actualSize;
        size_t alignedSize;
        size_t paddingWaste;
        float memoryEfficiency;
        bool isOptimal;
        std::string recommendations;
        
        PaddingAnalysis(const char* structName) {
            actualSize = sizeof(T);
            alignedSize = (actualSize + SIMD_ALIGNMENT - 1) & ~(SIMD_ALIGNMENT - 1);
            paddingWaste = alignedSize - actualSize;
            memoryEfficiency = (static_cast<float>(actualSize) / alignedSize) * 100.0f;
            isOptimal = (paddingWaste == 0);
            
            if (!isOptimal) {
                recommendations = "Consider reordering fields to eliminate " + 
                                std::to_string(paddingWaste) + " bytes of padding waste";
            } else {
                recommendations = "Optimal: Zero padding waste achieved";
            }
        }
    };

    /**
     * SIMD alignment validation for array fields
     */
    template<typename T, size_t N>
    void validateSIMDAlignment(const std::array<T, N>& array, const char* fieldName) {
        uintptr_t address = reinterpret_cast<uintptr_t>(array.data());
        
        if (address % SIMD_ALIGNMENT != 0) {
            throw std::runtime_error(
                std::string("SIMD alignment violation: ") + fieldName + 
                " is not " + std::to_string(SIMD_ALIGNMENT) + "-byte aligned"
            );
        }
        
        // Validate array size is suitable for SIMD operations
        if (N % 8 != 0) {
            std::cerr << "WARNING: " << fieldName << " size (" << N 
                      << ") is not optimal for AVX2 SIMD operations (should be multiple of 8)\n";
        }
    }

    /**
     * Comprehensive SoA structure validation
     */
    class SoAStructureValidator {
    public:
        // Validate CPUCellPhysics_SoA structure
        static void validateCellPhysicsStructure(const CPUCellPhysics_SoA& data) {
            // Check overall structure alignment
            static_assert(alignof(CPUCellPhysics_SoA) >= SIMD_ALIGNMENT, 
                         "CPUCellPhysics_SoA must have 32-byte alignment");
            
            // Validate individual array alignments
            validateSIMDAlignment(data.pos_x, "pos_x");
            validateSIMDAlignment(data.pos_y, "pos_y");
            validateSIMDAlignment(data.pos_z, "pos_z");
            validateSIMDAlignment(data.vel_x, "vel_x");
            validateSIMDAlignment(data.vel_y, "vel_y");
            validateSIMDAlignment(data.vel_z, "vel_z");
            validateSIMDAlignment(data.acc_x, "acc_x");
            validateSIMDAlignment(data.acc_y, "acc_y");
            validateSIMDAlignment(data.acc_z, "acc_z");
            validateSIMDAlignment(data.quat_x, "quat_x");
            validateSIMDAlignment(data.quat_y, "quat_y");
            validateSIMDAlignment(data.quat_z, "quat_z");
            validateSIMDAlignment(data.quat_w, "quat_w");
            validateSIMDAlignment(data.mass, "mass");
            validateSIMDAlignment(data.radius, "radius");
            validateSIMDAlignment(data.age, "age");
            validateSIMDAlignment(data.energy, "energy");
            validateSIMDAlignment(data.cellType, "cellType");
            validateSIMDAlignment(data.genomeID, "genomeID");
            validateSIMDAlignment(data.flags, "flags");
            
            // Validate active cell count bounds
            if (data.activeCellCount > MAX_CELLS) {
                throw std::runtime_error("Active cell count exceeds maximum: " + 
                                       std::to_string(data.activeCellCount) + " > " + 
                                       std::to_string(MAX_CELLS));
            }
        }

        // Validate CPUAdhesionConnections_SoA structure
        static void validateAdhesionConnectionsStructure(const CPUAdhesionConnections_SoA& data) {
            // Check overall structure alignment
            static_assert(alignof(CPUAdhesionConnections_SoA) >= SIMD_ALIGNMENT, 
                         "CPUAdhesionConnections_SoA must have 32-byte alignment");
            
            // Validate individual array alignments
            validateSIMDAlignment(data.cellAIndex, "cellAIndex");
            validateSIMDAlignment(data.cellBIndex, "cellBIndex");
            validateSIMDAlignment(data.modeIndex, "modeIndex");
            validateSIMDAlignment(data.isActive, "isActive");
            validateSIMDAlignment(data.zoneA, "zoneA");
            validateSIMDAlignment(data.zoneB, "zoneB");
            validateSIMDAlignment(data.anchorDirectionA_x, "anchorDirectionA_x");
            validateSIMDAlignment(data.anchorDirectionA_y, "anchorDirectionA_y");
            validateSIMDAlignment(data.anchorDirectionA_z, "anchorDirectionA_z");
            validateSIMDAlignment(data.anchorDirectionB_x, "anchorDirectionB_x");
            validateSIMDAlignment(data.anchorDirectionB_y, "anchorDirectionB_y");
            validateSIMDAlignment(data.anchorDirectionB_z, "anchorDirectionB_z");
            validateSIMDAlignment(data.twistReferenceA_x, "twistReferenceA_x");
            validateSIMDAlignment(data.twistReferenceA_y, "twistReferenceA_y");
            validateSIMDAlignment(data.twistReferenceA_z, "twistReferenceA_z");
            validateSIMDAlignment(data.twistReferenceA_w, "twistReferenceA_w");
            validateSIMDAlignment(data.twistReferenceB_x, "twistReferenceB_x");
            validateSIMDAlignment(data.twistReferenceB_y, "twistReferenceB_y");
            validateSIMDAlignment(data.twistReferenceB_z, "twistReferenceB_z");
            validateSIMDAlignment(data.twistReferenceB_w, "twistReferenceB_w");
            
            // Validate active connection count bounds
            if (data.activeConnectionCount > MAX_CONNECTIONS) {
                throw std::runtime_error("Active connection count exceeds maximum: " + 
                                       std::to_string(data.activeConnectionCount) + " > " + 
                                       std::to_string(MAX_CONNECTIONS));
            }
        }

        // Comprehensive padding analysis
        static void analyzePaddingEfficiency() {
            std::cout << "=== CPU SoA Structure Padding Analysis ===\n";
            
            // Analyze CPUCellPhysics_SoA
            PaddingAnalysis<CPUCellPhysics_SoA> cellAnalysis("CPUCellPhysics_SoA");
            std::cout << "CPUCellPhysics_SoA:\n";
            std::cout << "  Actual size: " << cellAnalysis.actualSize << " bytes\n";
            std::cout << "  Aligned size: " << cellAnalysis.alignedSize << " bytes\n";
            std::cout << "  Padding waste: " << cellAnalysis.paddingWaste << " bytes\n";
            std::cout << "  Memory efficiency: " << cellAnalysis.memoryEfficiency << "%\n";
            std::cout << "  Status: " << cellAnalysis.recommendations << "\n\n";
            
            // Analyze CPUAdhesionConnections_SoA
            PaddingAnalysis<CPUAdhesionConnections_SoA> adhesionAnalysis("CPUAdhesionConnections_SoA");
            std::cout << "CPUAdhesionConnections_SoA:\n";
            std::cout << "  Actual size: " << adhesionAnalysis.actualSize << " bytes\n";
            std::cout << "  Aligned size: " << adhesionAnalysis.alignedSize << " bytes\n";
            std::cout << "  Padding waste: " << adhesionAnalysis.paddingWaste << " bytes\n";
            std::cout << "  Memory efficiency: " << adhesionAnalysis.memoryEfficiency << "%\n";
            std::cout << "  Status: " << adhesionAnalysis.recommendations << "\n\n";
            
            // Analyze parameter structures
            PaddingAnalysis<CPUGenomeParameters> genomeAnalysis("CPUGenomeParameters");
            std::cout << "CPUGenomeParameters:\n";
            std::cout << "  Actual size: " << genomeAnalysis.actualSize << " bytes\n";
            std::cout << "  Memory efficiency: " << genomeAnalysis.memoryEfficiency << "%\n";
            std::cout << "  Status: " << genomeAnalysis.recommendations << "\n\n";
            
            PaddingAnalysis<CPUCellParameters> cellParamAnalysis("CPUCellParameters");
            std::cout << "CPUCellParameters:\n";
            std::cout << "  Actual size: " << cellParamAnalysis.actualSize << " bytes\n";
            std::cout << "  Memory efficiency: " << cellParamAnalysis.memoryEfficiency << "%\n";
            std::cout << "  Status: " << cellParamAnalysis.recommendations << "\n\n";
            
            // Overall assessment
            bool allOptimal = cellAnalysis.isOptimal && adhesionAnalysis.isOptimal;
            if (allOptimal) {
                std::cout << "✓ All SoA structures have optimal memory layout with zero padding waste\n";
            } else {
                std::cout << "⚠ Some structures have padding waste - consider field reordering\n";
            }
        }
    };

    /**
     * Data integrity validation for SoA structures
     */
    class SoADataIntegrityValidator {
    public:
        // Validate cell data integrity
        static void validateCellDataIntegrity(const CPUCellPhysics_SoA& data) {
            for (size_t i = 0; i < data.activeCellCount; ++i) {
                // Validate position values are finite
                if (!std::isfinite(data.pos_x[i]) || !std::isfinite(data.pos_y[i]) || !std::isfinite(data.pos_z[i])) {
                    throw std::runtime_error("Invalid position values at cell index " + std::to_string(i));
                }
                
                // Validate velocity values are finite
                if (!std::isfinite(data.vel_x[i]) || !std::isfinite(data.vel_y[i]) || !std::isfinite(data.vel_z[i])) {
                    throw std::runtime_error("Invalid velocity values at cell index " + std::to_string(i));
                }
                
                // Validate quaternion is normalized (within tolerance)
                float quat_length_sq = data.quat_x[i] * data.quat_x[i] + 
                                      data.quat_y[i] * data.quat_y[i] + 
                                      data.quat_z[i] * data.quat_z[i] + 
                                      data.quat_w[i] * data.quat_w[i];
                if (std::abs(quat_length_sq - 1.0f) > 1e-5f) {
                    throw std::runtime_error("Quaternion not normalized at cell index " + std::to_string(i));
                }
                
                // Validate physical properties are positive
                if (data.mass[i] <= 0.0f || data.radius[i] <= 0.0f) {
                    throw std::runtime_error("Invalid physical properties at cell index " + std::to_string(i));
                }
                
                // Validate age and energy are non-negative
                if (data.age[i] < 0.0f || data.energy[i] < 0.0f) {
                    throw std::runtime_error("Invalid age or energy at cell index " + std::to_string(i));
                }
            }
        }

        // Validate adhesion connection data integrity
        static void validateAdhesionDataIntegrity(const CPUAdhesionConnections_SoA& data, 
                                                 const CPUCellPhysics_SoA& cellData) {
            for (size_t i = 0; i < data.activeConnectionCount; ++i) {
                // Validate cell indices are within bounds
                uint32_t cellA = data.cellAIndex[i];
                uint32_t cellB = data.cellBIndex[i];
                
                if (cellA >= MAX_CELLS || cellB >= MAX_CELLS) {
                    throw std::runtime_error("Cell index out of bounds in connection " + std::to_string(i));
                }
                
                if (cellA == cellB) {
                    throw std::runtime_error("Self-connection detected at connection " + std::to_string(i));
                }
                
                // Validate connection is active
                if (data.isActive[i] == 0) {
                    throw std::runtime_error("Inactive connection found in active range at index " + std::to_string(i));
                }
                
                // Validate anchor directions are normalized
                float anchorA_length_sq = data.anchorDirectionA_x[i] * data.anchorDirectionA_x[i] + 
                                         data.anchorDirectionA_y[i] * data.anchorDirectionA_y[i] + 
                                         data.anchorDirectionA_z[i] * data.anchorDirectionA_z[i];
                
                float anchorB_length_sq = data.anchorDirectionB_x[i] * data.anchorDirectionB_x[i] + 
                                         data.anchorDirectionB_y[i] * data.anchorDirectionB_y[i] + 
                                         data.anchorDirectionB_z[i] * data.anchorDirectionB_z[i];
                
                if (std::abs(anchorA_length_sq - 1.0f) > 1e-5f) {
                    throw std::runtime_error("Anchor direction A not normalized at connection " + std::to_string(i));
                }
                
                if (std::abs(anchorB_length_sq - 1.0f) > 1e-5f) {
                    throw std::runtime_error("Anchor direction B not normalized at connection " + std::to_string(i));
                }
                
                // Validate zone classifications are valid (0=ZoneA, 1=ZoneB, 2=ZoneC)
                if (data.zoneA[i] > 2 || data.zoneB[i] > 2) {
                    throw std::runtime_error("Invalid zone classification at connection " + std::to_string(i));
                }
            }
        }

        // Comprehensive bounds checking
        static void validateBounds(const CPUCellPhysics_SoA& cellData, 
                                  const CPUAdhesionConnections_SoA& adhesionData) {
            // Validate array bounds
            if (cellData.activeCellCount > MAX_CELLS) {
                throw std::runtime_error("Active cell count exceeds maximum");
            }
            
            if (adhesionData.activeConnectionCount > MAX_CONNECTIONS) {
                throw std::runtime_error("Active connection count exceeds maximum");
            }
            
            // Validate all active cells have valid data
            for (size_t i = 0; i < cellData.activeCellCount; ++i) {
                if (cellData.mass[i] == 0.0f) {
                    throw std::runtime_error("Active cell has zero mass at index " + std::to_string(i));
                }
            }
            
            // Validate all connections reference active cells
            for (size_t i = 0; i < adhesionData.activeConnectionCount; ++i) {
                uint32_t cellA = adhesionData.cellAIndex[i];
                uint32_t cellB = adhesionData.cellBIndex[i];
                
                // Check if referenced cells are actually active
                bool cellA_active = (cellA < cellData.activeCellCount && cellData.mass[cellA] > 0.0f);
                bool cellB_active = (cellB < cellData.activeCellCount && cellData.mass[cellB] > 0.0f);
                
                if (!cellA_active || !cellB_active) {
                    throw std::runtime_error("Connection references inactive cell at connection " + std::to_string(i));
                }
            }
        }
    };

    /**
     * Additional validation utilities
     */
    void validateBoundsChecking(const CPUCellPhysics_SoA& cellData, 
                               const CPUAdhesionConnections_SoA& adhesionData);
    
    void validateNumericalStability(const CPUCellPhysics_SoA& cellData);
    
    void validateSIMDCompatibility();

    /**
     * Comprehensive validation functions
     */
    void runComprehensiveValidation(const CPUCellPhysics_SoA& cellData, 
                                   const CPUAdhesionConnections_SoA& adhesionData);

    /**
     * Analysis and reporting functions
     */
    void validateMemoryLayout();
    void performanceAnalysis();
    void printDetailedStructureInfo();

    /**
     * Static assertions for compile-time validation
     */
    namespace CompileTimeValidation {
        // Validate structure alignments
        static_assert(alignof(CPUCellPhysics_SoA) >= SIMD_ALIGNMENT, 
                     "CPUCellPhysics_SoA must be 32-byte aligned for SIMD operations");
        static_assert(alignof(CPUAdhesionConnections_SoA) >= SIMD_ALIGNMENT, 
                     "CPUAdhesionConnections_SoA must be 32-byte aligned for SIMD operations");
        
        // Validate array sizes are SIMD-friendly
        static_assert(MAX_CELLS % 8 == 0, "MAX_CELLS should be multiple of 8 for optimal SIMD processing");
        static_assert(MAX_CONNECTIONS % 8 == 0, "MAX_CONNECTIONS should be multiple of 8 for optimal SIMD processing");
        
        // Validate structure sizes are reasonable
        static_assert(sizeof(CPUCellPhysics_SoA) < 1024 * 1024, "CPUCellPhysics_SoA is too large");
        static_assert(sizeof(CPUAdhesionConnections_SoA) < 1024 * 1024, "CPUAdhesionConnections_SoA is too large");
        
        // Validate parameter structures are efficiently packed
        static_assert(sizeof(CPUGenomeParameters) <= 200, "CPUGenomeParameters should be cache-friendly (increased for child adhesion flags)");
        static_assert(sizeof(CPUCellParameters) <= 200, "CPUCellParameters should be cache-friendly (increased for child adhesion flags)");
        static_assert(sizeof(CPUAdhesionParameters) <= 80, "CPUAdhesionParameters should be cache-friendly");
    }

} // namespace CPUSoAValidation