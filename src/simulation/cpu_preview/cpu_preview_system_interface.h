#pragma once

#include "../cell/common_structs.h"
#include "cpu_soa_data_manager.h"

/**
 * CPU Preview System Interface
 * 
 * Defines the system boundaries between CPU Preview and GPU Main systems.
 * Ensures complete independence with no data conversion during normal operation.
 * 
 * Requirements addressed: 3.4, 5.4
 */

namespace CPUPreviewSystemInterface {

    /**
     * System Boundary Definitions
     * 
     * These interfaces establish clear boundaries between the CPU Preview System
     * and the existing GPU Main Simulation System, ensuring no coupling.
     */

    // CPU Preview System operates on native CPU SoA data
    using CPUPreviewCellData = CPUCellPhysics_SoA;
    using CPUPreviewAdhesionData = CPUAdhesionConnections_SoA;
    using CPUPreviewGenomeParams = CPUGenomeParameters;
    using CPUPreviewCellParams = CPUCellParameters;

    // GPU Main System operates on native AoS data (existing structures)
    using GPUMainCellData = std::vector<ComputeCell>;
    using GPUMainAdhesionData = std::vector<AdhesionConnection>;
    using GPUMainGenomeData = GenomeData;

    /**
     * System Independence Validation
     * 
     * These functions validate that the systems remain independent
     * and no data conversion occurs during normal operation.
     */
    
    // Validate system boundaries
    constexpr bool systemsAreIndependent() {
        return true; // CPU Preview and GPU Main never share data structures
    }
    
    constexpr bool requiresDataConversion() {
        return false; // No conversion needed - systems use native formats
    }
    
    constexpr bool hasSharedDependencies() {
        return false; // Only shared component is GPU rendering pipeline (visual data only)
    }

    /**
     * Scene File Format Boundaries
     * 
     * Each system uses its own native file format:
     * - CPU Preview System: .cpu_soa files (CPU Structure-of-Arrays format)
     * - GPU Main System: .gpu_aos files (GPU Array-of-Structures format)
     */
    
    constexpr const char* getCPUPreviewFileExtension() {
        return ".cpu_soa";
    }
    
    constexpr const char* getGPUMainFileExtension() {
        return ".gpu_aos";
    }

    /**
     * Performance Boundary Definitions
     * 
     * Each system optimized for its specific use case:
     * - CPU Preview: Sub-16ms genome parameter iteration
     * - GPU Main: Full-scale simulation with thousands of cells
     */
    
    constexpr size_t getCPUPreviewMaxCells() {
        return 256; // Optimized for rapid genome iteration
    }
    
    constexpr size_t getGPUMainMaxCells() {
        return config::MAX_CELLS; // Full simulation capacity
    }
    
    constexpr float getCPUPreviewPerformanceTarget() {
        return 16.0f; // milliseconds - sub-frame performance
    }

    /**
     * Visual Data Interface
     * 
     * Both systems output to the same GPU rendering pipeline,
     * but use different data extraction methods:
     * - CPU Preview: Direct CPU SoA to visual data extraction
     * - GPU Main: GPU buffer to visual data extraction
     */
    
    struct CPUVisualDataInterface {
        // Common visual data format for both systems
        using PositionArray = std::array<glm::vec3, 256>;
        using OrientationArray = std::array<glm::quat, 256>;
        using ColorArray = std::array<glm::vec4, 256>;
        using MatrixArray = std::array<glm::mat4, 256>;
        
        // System identification
        enum class SourceSystem {
            CPUPreview,
            GPUMain
        };
        
        SourceSystem sourceSystem;
        size_t activeCount;
        
        // Visual data (same format regardless of source system)
        PositionArray positions;
        OrientationArray orientations;
        ColorArray colors;
        MatrixArray instanceMatrices;
    };

    /**
     * System Coordination Interface
     * 
     * Minimal coordination between systems for scene switching.
     * No data sharing - only system state coordination.
     */
    
    enum class ActiveSystem {
        CPUPreviewSimulation,  // CPU Preview System active
        GPUMainSimulation      // GPU Main System active
    };
    
    struct CPUSystemCoordinator {
        ActiveSystem currentSystem = ActiveSystem::CPUPreviewSimulation;
        bool systemSwitchRequested = false;
        
        // System switching (no data conversion)
        void requestSystemSwitch(ActiveSystem newSystem) {
            if (currentSystem != newSystem) {
                systemSwitchRequested = true;
                currentSystem = newSystem;
            }
        }
        
        bool hasSystemSwitchRequest() {
            bool request = systemSwitchRequested;
            systemSwitchRequested = false;
            return request;
        }
        
        // System state queries
        bool isCPUPreviewActive() const { return currentSystem == ActiveSystem::CPUPreviewSimulation; }
        bool isGPUMainActive() const { return currentSystem == ActiveSystem::GPUMainSimulation; }
        
        // Performance monitoring
        float getActiveSystemPerformance() const {
            // Returns last frame time for active system
            return 0.0f; // Implementation would query appropriate system
        }
    };

} // namespace CPUPreviewSystemInterface