#include "cpu_adhesion_force_calculator.h"
#include <chrono>
#include <iostream>
#include <cmath>

CPUAdhesionForceCalculator::CPUAdhesionForceCalculator() {
    // Initialize performance tracking
    m_lastCalculationTime = 0.0f;
    m_processedConnectionCount = 0;
}

CPUAdhesionForceCalculator::~CPUAdhesionForceCalculator() {
    // Cleanup handled by RAII
}

void CPUAdhesionForceCalculator::computeAdhesionForces(
    const CPUAdhesionConnections_SoA& connections,
    CPUCellPhysics_SoA& cells,
    const std::vector<GPUModeAdhesionSettings>& modeSettings,
    float deltaTime) {
    
    auto startTime = std::chrono::steady_clock::now();
    
    m_processedConnectionCount = 0;
    
    // Process each active adhesion connection
    for (size_t i = 0; i < connections.activeConnectionCount; ++i) {
        // Skip inactive connections
        if (connections.isActive[i] == 0) {
            continue;
        }
        
        uint32_t cellAIndex = connections.cellAIndex[i];
        uint32_t cellBIndex = connections.cellBIndex[i];
        uint32_t modeIndex = connections.modeIndex[i];
        
        // Validate indices
        if (cellAIndex >= cells.activeCellCount || cellBIndex >= cells.activeCellCount) {
            continue;
        }
        
        if (modeIndex >= modeSettings.size()) {
            continue;
        }
        
        // Convert SoA data to GPU-compatible structures
        ComputeCell cellA = convertSoAToComputeCell(cells, cellAIndex);
        ComputeCell cellB = convertSoAToComputeCell(cells, cellBIndex);
        AdhesionConnection connection = convertSoAToAdhesionConnection(connections, static_cast<uint32_t>(i));
        const GPUModeAdhesionSettings& settings = modeSettings[modeIndex];
        
        // Calculate forces and torques using GPU-equivalent algorithm
        glm::vec3 forceA, torqueA, forceB, torqueB;
        computeAdhesionForces(cellA, cellB, settings, connection, forceA, torqueA, forceB, torqueB);
        
        // Apply forces to cells
        applyCellForces(cells, cellAIndex, forceA, torqueA, cellA.positionAndMass.w);
        applyCellForces(cells, cellBIndex, forceB, torqueB, cellB.positionAndMass.w);
        
        m_processedConnectionCount++;
    }
    
    // Update performance metrics
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    m_lastCalculationTime = duration.count() / 1000.0f; // Convert to milliseconds
}

void CPUAdhesionForceCalculator::computeAdhesionForces(
    const ComputeCell& A, const ComputeCell& B,
    const GPUModeAdhesionSettings& settings,
    const AdhesionConnection& connection,
    glm::vec3& forceA, glm::vec3& torqueA,
    glm::vec3& forceB, glm::vec3& torqueB) {
    
    // Initialize outputs (GPU algorithm)
    forceA = glm::vec3(0.0f);
    torqueA = glm::vec3(0.0f);
    forceB = glm::vec3(0.0f);
    torqueB = glm::vec3(0.0f);
    
    // Connection vector from A to B (GPU algorithm)
    glm::vec3 deltaPos = glm::vec3(B.positionAndMass) - glm::vec3(A.positionAndMass);
    float dist = glm::length(deltaPos);
    if (dist < QUATERNION_EPSILON) return;
    
    glm::vec3 adhesionDir = deltaPos / dist;
    float restLength = settings.restLength;
    
    // Linear spring force (GPU algorithm)
    float forceMag = settings.linearSpringStiffness * (dist - restLength);
    glm::vec3 springForce = adhesionDir * forceMag;

    // Damping - oppose relative motion (GPU algorithm)
    glm::vec3 relVel = glm::vec3(B.velocity) - glm::vec3(A.velocity);
    float dampMag = 1.0f - settings.linearSpringDamping * glm::dot(relVel, adhesionDir);
    glm::vec3 dampingForce = -adhesionDir * dampMag; // Negative to oppose motion

    forceA += springForce + dampingForce;
    forceB -= springForce + dampingForce;
    
    // Use the fixed anchor directions from the connection, transformed to world space (GPU algorithm)
    glm::vec3 anchorA, anchorB;
    if (glm::length(connection.anchorDirectionA) < ANGLE_EPSILON && 
        glm::length(connection.anchorDirectionB) < ANGLE_EPSILON) {
        // Fallback: use default directions (this should not happen in genome-based system)
        anchorA = glm::vec3(1.0f, 0.0f, 0.0f);
        anchorB = glm::vec3(-1.0f, 0.0f, 0.0f);
    } else {
        // Transform the stored local anchor directions to world space using cell orientations (GPU algorithm)
        glm::quat quatA(A.orientation.w, A.orientation.x, A.orientation.y, A.orientation.z);
        glm::quat quatB(B.orientation.w, B.orientation.x, B.orientation.y, B.orientation.z);
        anchorA = rotateVectorByQuaternion(connection.anchorDirectionA, quatA);
        anchorB = rotateVectorByQuaternion(connection.anchorDirectionB, quatB);
    }
    
    // Apply orientation spring and damping using the fixed anchor directions (GPU algorithm)
    glm::vec3 axisA = glm::cross(anchorA, adhesionDir);
    float sinA = glm::length(axisA);
    float cosA = glm::dot(anchorA, adhesionDir);
    float angleA = std::atan2(sinA, cosA);
    if (sinA > QUATERNION_EPSILON) {
        axisA = glm::normalize(axisA);
        // Orientation spring and damping (GPU algorithm)
        glm::vec3 springTorqueA = axisA * angleA * settings.orientationSpringStiffness;
        glm::vec3 dampingTorqueA = -axisA * glm::dot(glm::vec3(A.angularVelocity), axisA) * settings.orientationSpringDamping;
        torqueA += springTorqueA + dampingTorqueA;
    }

    glm::vec3 axisB = glm::cross(anchorB, -adhesionDir);
    float sinB = glm::length(axisB);
    float cosB = glm::dot(anchorB, -adhesionDir);
    float angleB = std::atan2(sinB, cosB);
    if (sinB > QUATERNION_EPSILON) {
        axisB = glm::normalize(axisB);
        // Orientation spring and damping (GPU algorithm)
        glm::vec3 springTorqueB = axisB * angleB * settings.orientationSpringStiffness;
        glm::vec3 dampingTorqueB = -axisB * glm::dot(glm::vec3(B.angularVelocity), axisB) * settings.orientationSpringDamping;
        torqueB += springTorqueB + dampingTorqueB;
    }

    // Apply twist constraints if enabled (GPU algorithm)
    if (settings.enableTwistConstraint != 0 && 
        glm::length(connection.twistReferenceA) > ANGLE_EPSILON && 
        glm::length(connection.twistReferenceB) > ANGLE_EPSILON) {
        
        // Calculate adhesion axis (from A to B) (GPU algorithm)
        glm::vec3 adhesionAxis = glm::normalize(deltaPos);
        
        // Calculate target orientations that maintain the original twist relationship (GPU algorithm)
        // but allow the anchor alignment to work properly
        
        // Get the current anchor directions in world space (GPU algorithm)
        glm::quat quatA(A.orientation.w, A.orientation.x, A.orientation.y, A.orientation.z);
        glm::quat quatB(B.orientation.w, B.orientation.x, B.orientation.y, B.orientation.z);
        glm::vec3 currentAnchorA = rotateVectorByQuaternion(connection.anchorDirectionA, quatA);
        glm::vec3 currentAnchorB = rotateVectorByQuaternion(connection.anchorDirectionB, quatB);
        
        // Calculate what the orientations should be if anchors were perfectly aligned (GPU algorithm)
        glm::vec3 targetAnchorA = adhesionAxis;
        glm::vec3 targetAnchorB = -adhesionAxis;
        
        // Find the rotation needed to align current anchor to target anchor (GPU algorithm)
        glm::quat alignmentRotA = quatFromTwoVectors(currentAnchorA, targetAnchorA);
        glm::quat alignmentRotB = quatFromTwoVectors(currentAnchorB, targetAnchorB);
        
        // Apply the alignment rotation to the reference orientations to get target orientations (GPU algorithm)
        // GPU uses normalize() directly on the quaternion multiplication result
        glm::quat targetOrientationA = normalizeQuaternion(quatMultiply(alignmentRotA, connection.twistReferenceA));
        glm::quat targetOrientationB = normalizeQuaternion(quatMultiply(alignmentRotB, connection.twistReferenceB));
        
        // Calculate the rotation needed to reach target orientations (GPU algorithm)
        glm::quat correctionRotA = normalizeQuaternion(quatMultiply(targetOrientationA, quatConjugate(quatA)));
        glm::quat correctionRotB = normalizeQuaternion(quatMultiply(targetOrientationB, quatConjugate(quatB)));
        
        // Convert to axis-angle and apply as torque (but only around adhesion axis) (GPU algorithm)
        glm::vec4 axisAngleA = quatToAxisAngle(correctionRotA);
        glm::vec4 axisAngleB = quatToAxisAngle(correctionRotB);
        
        // Project correction onto adhesion axis to get twist component only (GPU algorithm)
        float twistCorrectionA = axisAngleA.w * glm::dot(glm::vec3(axisAngleA), adhesionAxis);
        float twistCorrectionB = axisAngleB.w * glm::dot(glm::vec3(axisAngleB), adhesionAxis);
        
        // Apply twist correction with higher strength for snake body alignment (GPU algorithm)
        // Normalize the correction angles to prevent excessive torques
        twistCorrectionA = glm::clamp(twistCorrectionA, -TWIST_CLAMP_LIMIT, TWIST_CLAMP_LIMIT); // Limit to ±90 degrees
        twistCorrectionB = glm::clamp(twistCorrectionB, -TWIST_CLAMP_LIMIT, TWIST_CLAMP_LIMIT);
        
        // Reduced twist strength for CPU stability (GPU uses 0.3, we use 0.05)
        glm::vec3 twistTorqueA = adhesionAxis * twistCorrectionA * settings.twistConstraintStiffness * 0.05f;
        glm::vec3 twistTorqueB = adhesionAxis * twistCorrectionB * settings.twistConstraintStiffness * 0.05f;
        
        // Add strong damping to prevent oscillation and maintain stable snake body
        // Increased damping for CPU stability (GPU uses 0.4, we use 0.6)
        float angularVelA = glm::dot(glm::vec3(A.angularVelocity), adhesionAxis);
        float angularVelB = glm::dot(glm::vec3(B.angularVelocity), adhesionAxis);
        float relativeAngularVel = angularVelA - angularVelB;
        
        glm::vec3 twistDampingA = -adhesionAxis * relativeAngularVel * settings.twistConstraintDamping * 0.6f;
        glm::vec3 twistDampingB = adhesionAxis * relativeAngularVel * settings.twistConstraintDamping * 0.6f;
        
        torqueA += twistTorqueA + twistDampingA;
        torqueB += twistTorqueB + twistDampingB;
    }

    // Apply a tangential force from the torque (GPU algorithm)
    forceA += glm::cross(-deltaPos, torqueB);
    forceB += glm::cross(deltaPos, torqueA);

    // This conserves angular momentum but makes cells look less natural, maybe better to comment it out
    torqueA -= torqueB;
    torqueB -= torqueA;
}

// Quaternion mathematics functions (exact GPU ports)
glm::quat CPUAdhesionForceCalculator::quatMultiply(const glm::quat& q1, const glm::quat& q2) const {
    // GPU algorithm: quatMultiply(vec4 q1, vec4 q2)
    // Note: GPU shader returns vec4(x,y,z,w) but GLM quat constructor is (w,x,y,z)
    return glm::quat(
        q1.w*q2.w - q1.x*q2.x - q1.y*q2.y - q1.z*q2.z,  // w
        q1.w*q2.x + q1.x*q2.w + q1.y*q2.z - q1.z*q2.y,  // x
        q1.w*q2.y - q1.x*q2.z + q1.y*q2.w + q1.z*q2.x,  // y
        q1.w*q2.z + q1.x*q2.y - q1.y*q2.x + q1.z*q2.w   // z
    );
}

glm::quat CPUAdhesionForceCalculator::quatConjugate(const glm::quat& q) const {
    // GPU algorithm: quatConjugate(vec4 q) returns vec4(-q.xyz, q.w)
    return glm::quat(q.w, -q.x, -q.y, -q.z);
}

glm::quat CPUAdhesionForceCalculator::quatInverse(const glm::quat& q) const {
    // GPU algorithm: quatInverse(vec4 q)
    float norm = glm::dot(q, q);
    if (norm > 0.0f) {
        return quatConjugate(q) / norm;
    }
    // GPU returns vec4(0.0, 0.0, 0.0, 1.0) which is identity quaternion
    return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
}

glm::vec3 CPUAdhesionForceCalculator::rotateVectorByQuaternion(const glm::vec3& v, const glm::quat& q) const {
    // GPU algorithm: rotateVectorByQuaternion(vec3 v, vec4 q)
    glm::vec3 u = glm::vec3(q.x, q.y, q.z);
    float s = q.w;
    return 2.0f * glm::dot(u, v) * u + (s * s - glm::dot(u, u)) * v + 2.0f * s * glm::cross(u, v);
}

glm::vec4 CPUAdhesionForceCalculator::quatToAxisAngle(const glm::quat& q) const {
    // GPU algorithm: quatToAxisAngle(vec4 q)
    float angle = 2.0f * std::acos(glm::clamp(q.w, -1.0f, 1.0f));
    glm::vec3 axis;
    if (angle < 0.001f) { // GPU uses 0.001 threshold
        axis = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        axis = glm::normalize(glm::vec3(q.x, q.y, q.z) / std::sin(angle * 0.5f));
    }
    return glm::vec4(axis, angle);
}

glm::quat CPUAdhesionForceCalculator::axisAngleToQuat(const glm::vec4& axisAngle) const {
    // GPU algorithm: axisAngleToQuat(vec4 axisAngle)
    float halfAngle = axisAngle.w * 0.5f;
    float s = std::sin(halfAngle);
    // GPU returns vec4(axisAngle.xyz * s, cos(halfAngle))
    return glm::quat(std::cos(halfAngle), glm::vec3(axisAngle) * s);
}

glm::quat CPUAdhesionForceCalculator::quatFromTwoVectors(const glm::vec3& from, const glm::vec3& to) const {
    // GPU algorithm: quatFromTwoVectors(vec3 from, vec3 to)
    // Deterministic quaternion rotation from one vector to another
    // Uses a consistent method that avoids cross product ambiguity
    
    // Normalize inputs
    glm::vec3 v1 = glm::normalize(from);
    glm::vec3 v2 = glm::normalize(to);
    
    float cosAngle = glm::dot(v1, v2);
    
    // Vectors are already aligned - GPU uses 0.9999 threshold
    if (cosAngle > 0.9999f) {
        // GPU returns vec4(0.0, 0.0, 0.0, 1.0) which is identity quaternion
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity
    }
    
    // Vectors are opposite - use deterministic perpendicular axis - GPU uses -0.9999 threshold
    if (cosAngle < -0.9999f) {
        // Choose axis deterministically based on which component is smallest
        glm::vec3 axis;
        if (std::abs(v1.x) < std::abs(v1.y) && std::abs(v1.x) < std::abs(v1.z)) {
            axis = glm::normalize(glm::vec3(0.0f, -v1.z, v1.y));
        } else if (std::abs(v1.y) < std::abs(v1.z)) {
            axis = glm::normalize(glm::vec3(-v1.z, 0.0f, v1.x));
        } else {
            axis = glm::normalize(glm::vec3(-v1.y, v1.x, 0.0f));
        }
        // GPU returns vec4(axis, 0.0) which is 180 degree rotation
        return glm::quat(0.0f, axis.x, axis.y, axis.z); // 180 degree rotation
    }
    
    // General case: use half-way quaternion method (more stable than cross product)
    glm::vec3 halfway = glm::normalize(v1 + v2);
    glm::vec3 axis = glm::vec3(
        v1.y * halfway.z - v1.z * halfway.y,
        v1.z * halfway.x - v1.x * halfway.z,
        v1.x * halfway.y - v1.y * halfway.x
    );
    float w = glm::dot(v1, halfway);
    
    // GPU returns normalize(vec4(axis, w))
    return normalizeQuaternion(glm::quat(w, axis.x, axis.y, axis.z));
}

glm::quat CPUAdhesionForceCalculator::normalizeQuaternion(const glm::quat& q) const {
    // Proper quaternion normalization to prevent drift
    // Handle edge cases for numerical stability
    float norm = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    
    if (norm < QUATERNION_EPSILON) {
        // Degenerate quaternion - return identity
        return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    
    return glm::quat(q.w/norm, q.x/norm, q.y/norm, q.z/norm);
}

bool CPUAdhesionForceCalculator::validateQuaternionMathematics() const {
    bool allTestsPassed = true;
    
    // Test 1: Quaternion multiplication identity
    glm::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat testQuat(0.707f, 0.707f, 0.0f, 0.0f); // 90 degree rotation around X
    glm::quat result = quatMultiply(testQuat, identity);
    
    float error = glm::length(glm::vec4(result.x - testQuat.x, result.y - testQuat.y, 
                                       result.z - testQuat.z, result.w - testQuat.w));
    if (error > QUATERNION_EPSILON) {
        std::cerr << "Quaternion multiplication identity test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 2: Quaternion conjugate
    glm::quat conjugate = quatConjugate(testQuat);
    glm::quat expectedConjugate(testQuat.w, -testQuat.x, -testQuat.y, -testQuat.z);
    error = glm::length(glm::vec4(conjugate.x - expectedConjugate.x, conjugate.y - expectedConjugate.y,
                                 conjugate.z - expectedConjugate.z, conjugate.w - expectedConjugate.w));
    if (error > QUATERNION_EPSILON) {
        std::cerr << "Quaternion conjugate test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 3: Quaternion inverse
    glm::quat inverse = quatInverse(testQuat);
    glm::quat product = quatMultiply(testQuat, inverse);
    error = glm::length(glm::vec4(product.x - identity.x, product.y - identity.y,
                                 product.z - identity.z, product.w - identity.w));
    if (error > QUATERNION_EPSILON) {
        std::cerr << "Quaternion inverse test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 4: Vector rotation
    glm::vec3 testVector(1.0f, 0.0f, 0.0f);
    glm::quat rotationY(0.707f, 0.0f, 0.707f, 0.0f); // 90 degree rotation around Y
    glm::vec3 rotatedVector = rotateVectorByQuaternion(testVector, rotationY);
    glm::vec3 expectedResult(0.0f, 0.0f, -1.0f); // X rotated 90 degrees around Y should be -Z
    
    error = glm::length(rotatedVector - expectedResult);
    if (error > ANGLE_EPSILON) {
        std::cerr << "Vector rotation test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 5: Axis-angle conversion
    glm::vec4 axisAngle = quatToAxisAngle(rotationY);
    glm::quat reconstructed = axisAngleToQuat(axisAngle);
    error = glm::length(glm::vec4(reconstructed.x - rotationY.x, reconstructed.y - rotationY.y,
                                 reconstructed.z - rotationY.z, reconstructed.w - rotationY.w));
    if (error > QUATERNION_EPSILON) {
        std::cerr << "Axis-angle conversion test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 6: Two vector quaternion (aligned vectors)
    glm::vec3 from(1.0f, 0.0f, 0.0f);
    glm::vec3 to(1.0f, 0.0f, 0.0f);
    glm::quat alignedQuat = quatFromTwoVectors(from, to);
    error = glm::length(glm::vec4(alignedQuat.x - identity.x, alignedQuat.y - identity.y,
                                 alignedQuat.z - identity.z, alignedQuat.w - identity.w));
    if (error > QUATERNION_EPSILON) {
        std::cerr << "Two vector quaternion (aligned) test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 7: Two vector quaternion (opposite vectors)
    glm::vec3 opposite(-1.0f, 0.0f, 0.0f);
    glm::quat oppositeQuat = quatFromTwoVectors(from, opposite);
    glm::vec3 rotatedOpposite = rotateVectorByQuaternion(from, oppositeQuat);
    error = glm::length(rotatedOpposite - opposite);
    if (error > ANGLE_EPSILON) {
        std::cerr << "Two vector quaternion (opposite) test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    // Test 8: Quaternion normalization
    glm::quat unnormalized(2.0f, 1.0f, 1.0f, 1.0f);
    glm::quat normalized = normalizeQuaternion(unnormalized);
    float norm = std::sqrt(normalized.w*normalized.w + normalized.x*normalized.x + 
                          normalized.y*normalized.y + normalized.z*normalized.z);
    if (std::abs(norm - 1.0f) > QUATERNION_EPSILON) {
        std::cerr << "Quaternion normalization test failed: norm = " << norm << std::endl;
        allTestsPassed = false;
    }
    
    // Test 9: Edge case - near-zero quaternion
    glm::quat nearZero(QUATERNION_EPSILON * 0.1f, 0.0f, 0.0f, 0.0f);
    glm::quat normalizedZero = normalizeQuaternion(nearZero);
    error = glm::length(glm::vec4(normalizedZero.x - identity.x, normalizedZero.y - identity.y,
                                 normalizedZero.z - identity.z, normalizedZero.w - identity.w));
    if (error > QUATERNION_EPSILON) {
        std::cerr << "Near-zero quaternion normalization test failed: error = " << error << std::endl;
        allTestsPassed = false;
    }
    
    if (allTestsPassed) {
        std::cout << "All quaternion mathematics tests passed!" << std::endl;
    }
    
    return allTestsPassed;
}

float CPUAdhesionForceCalculator::getRadius(const ComputeCell& cell) {
    // GPU algorithm: getRadius(ComputeCell cell)
    float mass = cell.positionAndMass.w; // mass is stored in w component 
    return std::pow(mass, 1.0f/3.0f);
}

ComputeCell CPUAdhesionForceCalculator::convertSoAToComputeCell(const CPUCellPhysics_SoA& cells, uint32_t index) {
    ComputeCell cell;
    
    // Physics data
    cell.positionAndMass = glm::vec4(cells.pos_x[index], cells.pos_y[index], cells.pos_z[index], cells.mass[index]);
    cell.velocity = glm::vec4(cells.vel_x[index], cells.vel_y[index], cells.vel_z[index], 0.0f);
    cell.acceleration = glm::vec4(cells.acc_x[index], cells.acc_y[index], cells.acc_z[index], 0.0f);
    // Ensure quaternion is normalized to prevent drift
    glm::quat orientation = glm::quat(cells.quat_w[index], cells.quat_x[index], cells.quat_y[index], cells.quat_z[index]);
    float norm = std::sqrt(orientation.w*orientation.w + orientation.x*orientation.x + orientation.y*orientation.y + orientation.z*orientation.z);
    if (norm > QUATERNION_EPSILON) {
        orientation = glm::quat(orientation.w/norm, orientation.x/norm, orientation.y/norm, orientation.z/norm);
    } else {
        orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Identity if degenerate
    }
    cell.orientation = glm::vec4(orientation.x, orientation.y, orientation.z, orientation.w);
    
    // Angular motion data
    cell.angularVelocity = glm::vec4(cells.angularVel_x[index], cells.angularVel_y[index], cells.angularVel_z[index], 0.0f);
    cell.angularAcceleration = glm::vec4(cells.angularAcc_x[index], cells.angularAcc_y[index], cells.angularAcc_z[index], 0.0f);
    cell.prevAngularAcceleration = glm::vec4(0.0f); // Not stored in SoA, use zero
    
    // Other properties
    cell.age = cells.age[index];
    
    return cell;
}

AdhesionConnection CPUAdhesionForceCalculator::convertSoAToAdhesionConnection(const CPUAdhesionConnections_SoA& connections, uint32_t index) {
    AdhesionConnection connection;
    
    connection.cellAIndex = connections.cellAIndex[index];
    connection.cellBIndex = connections.cellBIndex[index];
    connection.modeIndex = connections.modeIndex[index];
    connection.isActive = connections.isActive[index];
    connection.zoneA = connections.zoneA[index];
    connection.zoneB = connections.zoneB[index];
    
    connection.anchorDirectionA = glm::vec3(
        connections.anchorDirectionA_x[index],
        connections.anchorDirectionA_y[index],
        connections.anchorDirectionA_z[index]
    );
    
    connection.anchorDirectionB = glm::vec3(
        connections.anchorDirectionB_x[index],
        connections.anchorDirectionB_y[index],
        connections.anchorDirectionB_z[index]
    );
    
    // Ensure twist reference quaternions are normalized to prevent drift
    glm::quat twistRefA = glm::quat(
        connections.twistReferenceA_w[index],
        connections.twistReferenceA_x[index],
        connections.twistReferenceA_y[index],
        connections.twistReferenceA_z[index]
    );
    float normA = std::sqrt(twistRefA.w*twistRefA.w + twistRefA.x*twistRefA.x + twistRefA.y*twistRefA.y + twistRefA.z*twistRefA.z);
    if (normA > QUATERNION_EPSILON) {
        connection.twistReferenceA = glm::quat(twistRefA.w/normA, twistRefA.x/normA, twistRefA.y/normA, twistRefA.z/normA);
    } else {
        connection.twistReferenceA = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    
    glm::quat twistRefB = glm::quat(
        connections.twistReferenceB_w[index],
        connections.twistReferenceB_x[index],
        connections.twistReferenceB_y[index],
        connections.twistReferenceB_z[index]
    );
    float normB = std::sqrt(twistRefB.w*twistRefB.w + twistRefB.x*twistRefB.x + twistRefB.y*twistRefB.y + twistRefB.z*twistRefB.z);
    if (normB > QUATERNION_EPSILON) {
        connection.twistReferenceB = glm::quat(twistRefB.w/normB, twistRefB.x/normB, twistRefB.y/normB, twistRefB.z/normB);
    } else {
        connection.twistReferenceB = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    }
    
    return connection;
}

void CPUAdhesionForceCalculator::applyCellForces(CPUCellPhysics_SoA& cells, uint32_t cellIndex, 
                                                const glm::vec3& force, const glm::vec3& torque, float mass) {
    // Apply linear force as acceleration (F = ma, so a = F/m)
    glm::vec3 acceleration = force / mass;
    cells.acc_x[cellIndex] += acceleration.x;
    cells.acc_y[cellIndex] += acceleration.y;
    cells.acc_z[cellIndex] += acceleration.z;
    
    // Apply torque as angular acceleration (τ = Iα, so α = τ/I)
    // For sphere: I = 2/5 * m * r^2
    float radius = std::pow(mass, 1.0f/3.0f);
    float momentOfInertia = 0.4f * mass * radius * radius;
    
    if (momentOfInertia > EPSILON) {
        glm::vec3 angularAcceleration = torque / momentOfInertia;
        
        // Apply angular acceleration to SoA structure
        cells.angularAcc_x[cellIndex] += angularAcceleration.x;
        cells.angularAcc_y[cellIndex] += angularAcceleration.y;
        cells.angularAcc_z[cellIndex] += angularAcceleration.z;
    }
}