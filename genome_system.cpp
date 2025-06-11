#include "genome_system.h"
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <cmath>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Global genome system instance
std::unique_ptr<GenomeSystem> g_genomeSystem;

// GenomeMode implementation
bool GenomeMode::isValid() const {
    return splitInterval >= 1.0f && splitInterval <= 15.0f &&
           parentSplitYaw >= -180.0f && parentSplitYaw <= 180.0f &&
           parentSplitPitch >= -90.0f && parentSplitPitch <= 90.0f &&
           parentSplitRoll >= -180.0f && parentSplitRoll <= 180.0f &&
           childA_OrientationYaw >= -180.0f && childA_OrientationYaw <= 180.0f &&
           childA_OrientationPitch >= -90.0f && childA_OrientationPitch <= 90.0f &&
           childA_OrientationRoll >= -180.0f && childA_OrientationRoll <= 180.0f &&
           childB_OrientationYaw >= -180.0f && childB_OrientationYaw <= 180.0f &&
           childB_OrientationPitch >= -90.0f && childB_OrientationPitch <= 90.0f &&
           childB_OrientationRoll >= -180.0f && childB_OrientationRoll <= 180.0f &&
           adhesionRestLength >= 1.0f && adhesionRestLength <= 10.0f &&
           adhesionSpringStiffness >= 10.0f && adhesionSpringStiffness <= 500.0f &&
           adhesionSpringDamping >= 0.0f && adhesionSpringDamping <= 100.0f &&
           orientationConstraintStrength >= 0.0f && orientationConstraintStrength <= 1.0f &&
           maxAllowedAngleDeviation >= 0.0f && maxAllowedAngleDeviation <= 180.0f &&
           adhesionBreakForce >= 100.0f && adhesionBreakForce <= 5000.0f;
}

void GenomeMode::clampValues() {
    splitInterval = std::clamp(splitInterval, 1.0f, 15.0f);
    parentSplitYaw = std::clamp(parentSplitYaw, -180.0f, 180.0f);
    parentSplitPitch = std::clamp(parentSplitPitch, -90.0f, 90.0f);
    parentSplitRoll = std::clamp(parentSplitRoll, -180.0f, 180.0f);
    childA_OrientationYaw = std::clamp(childA_OrientationYaw, -180.0f, 180.0f);
    childA_OrientationPitch = std::clamp(childA_OrientationPitch, -90.0f, 90.0f);
    childA_OrientationRoll = std::clamp(childA_OrientationRoll, -180.0f, 180.0f);
    childB_OrientationYaw = std::clamp(childB_OrientationYaw, -180.0f, 180.0f);
    childB_OrientationPitch = std::clamp(childB_OrientationPitch, -90.0f, 90.0f);
    childB_OrientationRoll = std::clamp(childB_OrientationRoll, -180.0f, 180.0f);
    adhesionRestLength = std::clamp(adhesionRestLength, 1.0f, 10.0f);
    adhesionSpringStiffness = std::clamp(adhesionSpringStiffness, 10.0f, 500.0f);
    adhesionSpringDamping = std::clamp(adhesionSpringDamping, 0.0f, 100.0f);
    orientationConstraintStrength = std::clamp(orientationConstraintStrength, 0.0f, 1.0f);
    maxAllowedAngleDeviation = std::clamp(maxAllowedAngleDeviation, 0.0f, 180.0f);
    adhesionBreakForce = std::clamp(adhesionBreakForce, 100.0f, 5000.0f);
    
    // std::clamp color components
    modeColor.r = std::clamp(modeColor.r, 0.0f, 1.0f);
    modeColor.g = std::clamp(modeColor.g, 0.0f, 1.0f);
    modeColor.b = std::clamp(modeColor.b, 0.0f, 1.0f);
    modeColor.a = std::clamp(modeColor.a, 0.0f, 1.0f);
}

// GPUGenomeMode implementation
GPUGenomeMode GPUGenomeMode::fromGenomeMode(const GenomeMode& mode) {
    GPUGenomeMode gpuMode;
    
    // Basic data
    gpuMode.colorPacked = mode.modeColor.pack();
    gpuMode.splitInterval = mode.splitInterval;
      // Split and orientation data
    gpuMode.parentSplitYaw = mode.parentSplitYaw;
    gpuMode.parentSplitPitch = mode.parentSplitPitch;
    gpuMode.parentSplitRoll = mode.parentSplitRoll;
    gpuMode.childA_OrientationYaw = mode.childA_OrientationYaw;
    gpuMode.childA_OrientationPitch = mode.childA_OrientationPitch;
    gpuMode.childA_OrientationRoll = mode.childA_OrientationRoll;
    gpuMode.childB_OrientationYaw = mode.childB_OrientationYaw;
    gpuMode.childB_OrientationPitch = mode.childB_OrientationPitch;
    gpuMode.childB_OrientationRoll = mode.childB_OrientationRoll;
    
    // Adhesion parameters
    gpuMode.adhesionRestLength = mode.adhesionRestLength;
    gpuMode.adhesionSpringStiffness = mode.adhesionSpringStiffness;
    gpuMode.adhesionSpringDamping = mode.adhesionSpringDamping;
    gpuMode.adhesionBreakForce = mode.adhesionBreakForce;
    
    // Orientation constraints
    gpuMode.orientationConstraintStrength = mode.orientationConstraintStrength;
    gpuMode.maxAllowedAngleDeviation = mode.maxAllowedAngleDeviation;
    
    // Mode indices
    gpuMode.childAModeIndex = mode.childAModeIndex;
    gpuMode.childBModeIndex = mode.childBModeIndex;
    
    // Pack boolean flags
    gpuMode.flags = 0;
    gpuMode.setFlag(FLAG_IS_INITIAL, false); // Deal with this properly later
    gpuMode.setFlag(FLAG_PARENT_MAKE_ADHESION, mode.parentMakeAdhesion);
    gpuMode.setFlag(FLAG_CHILD_A_KEEP_ADHESION, mode.childA_KeepAdhesion);
    gpuMode.setFlag(FLAG_CHILD_B_KEEP_ADHESION, mode.childB_KeepAdhesion);
    gpuMode.setFlag(FLAG_ADHESION_CAN_BREAK, mode.adhesionCanBreak);
    
    return gpuMode;
}

// GenomeSystem implementation
GenomeSystem::GenomeSystem() {
    // Load default modes on construction
    loadDefaultModes();
}

void GenomeSystem::addMode(const GenomeMode& mode) {
    GenomeMode newMode = mode;
	//newMode.std::clampValues(); // This isn't needed because the default mode is already std::clamped
    
    modes.push_back(newMode);
    refreshModeIndices();
    updateGPUData();
    triggerGenomeChanged();
}

void GenomeSystem::removeMode(int index) {
    validateModeIndex(index);
    modes.erase(modes.begin() + index);
	initialModeIndex -= (initialModeIndex > index) ? 1 : 0; // Adjust initial mode index if necessary
    refreshModeIndices();
    updateGPUData();
    triggerGenomeChanged();
}

void GenomeSystem::updateMode(int index, const GenomeMode& mode) {
    validateModeIndex(index);
    
    GenomeMode updatedMode = mode;
    updatedMode.clampValues();
    updatedMode.index = index;

    modes[index] = updatedMode;
    updateGPUMode(index);
    triggerGenomeChanged();
}

GenomeMode* GenomeSystem::getMode(int index) {
    if (index < 0 || index >= static_cast<int>(modes.size())) {
        return nullptr;
    }
    return &modes[index];
}

const GenomeMode* GenomeSystem::getMode(int index) const {
    if (index < 0 || index >= static_cast<int>(modes.size())) {
        return nullptr;
    }
    return &modes[index];
}

void GenomeSystem::clearModes() {
    modes.clear();
    gpuModes.clear();
    triggerGenomeChanged();
}

void GenomeSystem::loadDefaultModes() {
    clearModes();    // Create only one default initial mode with all zero values
    GenomeMode defaultMode;
    defaultMode.modeName = "Default Mode";
    defaultMode.modeColor = Color(1.0f, 1.0f, 1.0f, 1.0f); // White with full alpha
    defaultMode.splitInterval = 5.0f; // Default split timer is 5 seconds
    defaultMode.parentSplitYaw = 0.0f;
    defaultMode.parentSplitPitch = 0.0f;
    defaultMode.parentSplitRoll = 0.0f;
    defaultMode.childA_OrientationYaw = 0.0f;
    defaultMode.childA_OrientationPitch = 0.0f;
    defaultMode.childA_OrientationRoll = 0.0f;
    defaultMode.childB_OrientationYaw = 0.0f;
    defaultMode.childB_OrientationPitch = 0.0f;
    defaultMode.childB_OrientationRoll = 0.0f;
    defaultMode.childAModeIndex = -1; // Use parent mode
    defaultMode.childBModeIndex = -1; // Use parent mode
    defaultMode.adhesionRestLength = 1.0f; // Minimum allowed value (can't be 0)
    defaultMode.adhesionSpringStiffness = 10.0f; // Minimum allowed value (can't be 0)
    defaultMode.adhesionSpringDamping = 0.0f;
    defaultMode.orientationConstraintStrength = 0.0f;
    defaultMode.maxAllowedAngleDeviation = 0.0f;
    defaultMode.parentMakeAdhesion = false;
    defaultMode.childA_KeepAdhesion = false;
    defaultMode.childB_KeepAdhesion = false;
    defaultMode.adhesionCanBreak = false;
    defaultMode.adhesionBreakForce = 100.0f; // Minimum allowed value (can't be 0)
    
    addMode(defaultMode);
}

void GenomeSystem::refreshModeIndices() {
    for (size_t i = 0; i < modes.size(); ++i) {
        modes[i].index = static_cast<int>(i);
        if (modes[i].modeName.empty()) {
            modes[i].modeName = "Mode " + std::to_string(i);
        }
    }
}

int GenomeSystem::getInitialModeIndex() const {
    return this->initialModeIndex;
}

void GenomeSystem::updateGPUData() {
    gpuModes.clear();
    gpuModes.reserve(modes.size());
    
    for (const auto& mode : modes) {
        gpuModes.push_back(GPUGenomeMode::fromGenomeMode(mode));
    }
}

void GenomeSystem::updateGPUMode(int index) {
    if (index >= 0 && index < static_cast<int>(gpuModes.size())) {
        gpuModes[index] = GPUGenomeMode::fromGenomeMode(modes[index]);
    }
}

void GenomeSystem::addChangeCallback(const GenomeChangeCallback& callback) {
    callbacks.push_back(callback);
}

void GenomeSystem::removeChangeCallback(const GenomeChangeCallback& callback) {
    // Note: This is a simplified implementation
    // In practice, you might want to use std::function comparison or callback IDs
}

void GenomeSystem::triggerGenomeChanged() {
    for (const auto& callback : callbacks) {
        callback();
    }
}

void GenomeSystem::validateModeIndex(int index) const {
    if (index < 0 || index >= static_cast<int>(modes.size())) {
        throw std::out_of_range("Mode index out of range: " + std::to_string(index));
    }
}

bool GenomeSystem::saveToFile(const std::string& filename) const {
    // TODO: Implement JSON or binary serialization
    return false;
}

bool GenomeSystem::loadFromFile(const std::string& filename) {
    // TODO: Implement JSON or binary deserialization
    return false;
}

// GenomeUtils implementation
namespace GenomeUtils {
    float degreesToRadians(float degrees) {
        return degrees * (static_cast<float>(M_PI) / 180.0f);
    }
    
    float radiansToDegrees(float radians) {
        return radians * (180.0f / static_cast<float>(M_PI));
    }
      glm::vec3 getDirectionFromYawPitch(float yaw, float pitch) {
        float yawRad = degreesToRadians(yaw);
        float pitchRad = degreesToRadians(pitch);
        
        return glm::vec3(
            cos(pitchRad) * sin(yawRad),
            sin(pitchRad),
            cos(pitchRad) * cos(yawRad)
        );
    }
    
    glm::vec3 getDirectionFromYawPitchRoll(float yaw, float pitch, float roll) {
        float yawRad = degreesToRadians(yaw);
        float pitchRad = degreesToRadians(pitch);
        float rollRad = degreesToRadians(roll);
        
        // Create rotation matrix from yaw, pitch, roll (ZYX order)
        glm::mat3 rotationMatrix = glm::mat3(
            cos(yawRad) * cos(pitchRad),
            sin(yawRad) * cos(pitchRad),
            -sin(pitchRad),
            
            cos(yawRad) * sin(pitchRad) * sin(rollRad) - sin(yawRad) * cos(rollRad),
            sin(yawRad) * sin(pitchRad) * sin(rollRad) + cos(yawRad) * cos(rollRad),
            cos(pitchRad) * sin(rollRad),
            
            cos(yawRad) * sin(pitchRad) * cos(rollRad) + sin(yawRad) * sin(rollRad),
            sin(yawRad) * sin(pitchRad) * cos(rollRad) - cos(yawRad) * sin(rollRad),
            cos(pitchRad) * cos(rollRad)
        );
        
        // Forward direction (Z-axis)
        return glm::vec3(rotationMatrix[0][2], rotationMatrix[1][2], rotationMatrix[2][2]);
    }
    
    glm::mat3 getRotationMatrixFromYawPitchRoll(float yaw, float pitch, float roll) {
        float yawRad = degreesToRadians(yaw);
        float pitchRad = degreesToRadians(pitch);
        float rollRad = degreesToRadians(roll);
        
        // Create rotation matrix from yaw, pitch, roll (ZYX order)
        return glm::mat3(
            cos(yawRad) * cos(pitchRad),
            sin(yawRad) * cos(pitchRad),
            -sin(pitchRad),
            
            cos(yawRad) * sin(pitchRad) * sin(rollRad) - sin(yawRad) * cos(rollRad),
            sin(yawRad) * sin(pitchRad) * sin(rollRad) + cos(yawRad) * cos(rollRad),
            cos(pitchRad) * sin(rollRad),
            
            cos(yawRad) * sin(pitchRad) * cos(rollRad) + sin(yawRad) * sin(rollRad),
            sin(yawRad) * sin(pitchRad) * cos(rollRad) - cos(yawRad) * sin(rollRad),
            cos(pitchRad) * cos(rollRad)
        );
    }
      Color lerpColor(const Color& a, const Color& b, float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return Color(
            a.r + (b.r - a.r) * t,
            a.g + (b.g - a.g) * t,
            a.b + (b.b - a.b) * t,
            a.a + (b.a - a.a) * t
        );
    }
    
    Color randomColor() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<float> dis(0.0f, 1.0f);
        
        return Color(dis(gen), dis(gen), dis(gen), 1.0f);
    }
    
    bool isValidSplitInterval(float interval) {
        return interval >= 1.0f && interval <= 15.0f;
    }
    
    bool isValidAngle(float angle, float minAngle, float maxAngle) {
        return angle >= minAngle && angle <= maxAngle;
    }
    
    bool isValidModeIndex(int index, int maxModes) {
        return index >= -1 && index < maxModes;
    }
}
