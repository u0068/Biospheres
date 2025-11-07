#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vector>
#include <cmath>
#include <algorithm>
#include "cpu_soa_data_manager.h"
#include "../cell/common_structs.h"

/**
 * CPU Adhesion Force Calculator
 * 
 * Direct port of GPU adhesion_physics.comp computeAdhesionForces function.
 * Implements complete adhesion physics with behavioral equivalence to GPU within 1e-6 tolerance.
 * Includes all quaternion mathematics, orientation constraints, and twist prevention.
 * 
 * Requirements addressed: 1.1, 1.2, 1.3, 1.4, 1.5, 2.1, 2.2, 2.3, 2.4, 2.5, 3.1, 3.2, 3.3, 3.4, 3.5, 6.1, 6.2, 6.3, 6.4, 6.5
 */
class CPUAdhesionForceCalculator {
public:
    CPUAdhesionForceCalculator();
    ~CPUAdhesionForceCalculator();

    // Main force calculation function (GPU-equivalent)
    void computeAdhesionForces(
        const CPUAdhesionConnections_SoA& connections,
        CPUCellPhysics_SoA& cells,
        const std::vector<GPUModeAdhesionSettings>& modeSettings,
        float deltaTime
    );

    // Performance monitoring
    float getLastCalculationTime() const { return m_lastCalculationTime; }
    size_t getProcessedConnectionCount() const { return m_processedConnectionCount; }

    // Validation and testing
    struct ValidationMetrics {
        float maxForceError = 0.0f;
        float maxTorqueError = 0.0f;
        float energyDifference = 0.0f;
        bool withinTolerance = true;
    };
    
    ValidationMetrics getLastValidationMetrics() const { return m_lastValidation; }
    
    // Quaternion mathematics validation
    bool validateQuaternionMathematics() const;

private:
    // Core GPU-equivalent force calculation (exact port of GPU computeAdhesionForces)
    void computeAdhesionForces(
        const ComputeCell& cellA, const ComputeCell& cellB,
        const GPUModeAdhesionSettings& settings,
        const AdhesionConnection& connection,
        glm::vec3& forceA, glm::vec3& torqueA,
        glm::vec3& forceB, glm::vec3& torqueB
    );

    // Quaternion mathematics functions (exact GPU ports)
    glm::quat quatMultiply(const glm::quat& q1, const glm::quat& q2) const;
    glm::quat quatConjugate(const glm::quat& q) const;
    glm::quat quatInverse(const glm::quat& q) const;
    glm::vec3 rotateVectorByQuaternion(const glm::vec3& v, const glm::quat& q) const;
    glm::vec4 quatToAxisAngle(const glm::quat& q) const;
    glm::quat axisAngleToQuat(const glm::vec4& axisAngle) const;
    glm::quat quatFromTwoVectors(const glm::vec3& from, const glm::vec3& to) const;
    glm::quat normalizeQuaternion(const glm::quat& q) const;

    // Helper functions
    float getRadius(const ComputeCell& cell);
    ComputeCell convertSoAToComputeCell(const CPUCellPhysics_SoA& cells, uint32_t index);
    AdhesionConnection convertSoAToAdhesionConnection(const CPUAdhesionConnections_SoA& connections, uint32_t index);
    void applyCellForces(CPUCellPhysics_SoA& cells, uint32_t cellIndex, 
                        const glm::vec3& force, const glm::vec3& torque, float mass);

    // Performance tracking
    float m_lastCalculationTime = 0.0f;
    size_t m_processedConnectionCount = 0;
    ValidationMetrics m_lastValidation;

    // Numerical precision constants (matching GPU)
    static constexpr float EPSILON = 1e-6f;
    static constexpr float ANGLE_EPSILON = 0.001f;
    static constexpr float QUATERNION_EPSILON = 0.0001f;
    static constexpr float TWIST_CLAMP_LIMIT = 1.57f; // ±90 degrees
};