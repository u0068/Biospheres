#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "cpu_soa_data_manager.h"
#include "../cell/common_structs.h"

/**
 * CPU Validation Harness for Behavioral Equivalence Testing
 * 
 * Ensures CPU Preview System maintains behavioral equivalence with GPU Main System.
 * Validates physics algorithms within documented tolerances.
 * 
 * Requirements addressed: 2.1, 2.2, 2.3, 2.4
 */
class CPUValidationHarness {
public:
    // Validation result structure
    struct ValidationResult {
        float maxPositionError = 0.0f;
        float maxVelocityError = 0.0f;
        float adhesionEnergyDifference = 0.0f;
        bool withinTolerance = true;
        std::string detailedReport;
        
        // Timing information
        float cpuSimulationTime = 0.0f;
        float gpuSimulationTime = 0.0f;
        
        // Statistical information
        size_t cellCount = 0;
        size_t connectionCount = 0;
        int simulationSteps = 0;
    };

    CPUValidationHarness();
    ~CPUValidationHarness();

    // Main validation interface
    ValidationResult compareImplementations(
        const CPUCellPhysics_SoA& cpuResult,
        const std::vector<ComputeCell>& gpuResult,
        int simulationSteps
    );

    // Automated testing
    void runRegressionTests();
    void generateTestScenarios();
    
    // Performance validation
    bool validatePerformanceTarget(float actualTime, float targetTime = 16.0f);
    
    // Configuration
    void setPositionTolerance(float tolerance) { m_positionTolerance = tolerance; }
    void setAdhesionEnergyTolerance(float tolerance) { m_adhesionEnergyTolerance = tolerance; }
    
    float getPositionTolerance() const { return m_positionTolerance; }
    float getAdhesionEnergyTolerance() const { return m_adhesionEnergyTolerance; }

    // Test scenario generation
    struct CPUTestScenario {
        std::string name;
        std::vector<CPUCellParameters> initialCells;
        std::vector<CPUAdhesionParameters> initialConnections;
        int simulationSteps;
        float deltaTime;
    };
    
    std::vector<CPUTestScenario> generateStandardTestScenarios();
    CPUTestScenario generateCollisionTestScenario();
    CPUTestScenario generateAdhesionTestScenario();
    CPUTestScenario generateBoundaryTestScenario();

private:
    // Tolerance constants (matching requirements)
    float m_positionTolerance = 1e-3f;      // 1e-3 units maximum position error
    float m_adhesionEnergyTolerance = 0.005f; // 0.5% adhesion energy difference
    
    // Validation methods
    float calculatePositionError(const glm::vec3& cpu, const glm::vec3& gpu);
    float calculateVelocityError(const glm::vec3& cpu, const glm::vec3& gpu);
    float calculateAdhesionEnergy(const CPUAdhesionConnections_SoA& connections,
                                 const CPUCellPhysics_SoA& cells);
    
    // Data conversion for comparison (only used in validation)
    std::vector<ComputeCell> convertSoAToAoS(const CPUCellPhysics_SoA& soaData);
    CPUCellPhysics_SoA convertAoSToSoA(const std::vector<ComputeCell>& aosData);
    
    // Report generation
    std::string generateDetailedReport(const ValidationResult& result);
    void logValidationResult(const ValidationResult& result);
    
    // Test data management
    void saveTestScenario(const CPUTestScenario& scenario, const std::string& filename);
    CPUTestScenario loadTestScenario(const std::string& filename);
    
    // Performance tracking
    std::vector<ValidationResult> m_validationHistory;
    std::chrono::steady_clock::time_point m_testStart;
};