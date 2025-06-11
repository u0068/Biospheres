#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "glm.hpp"

// Forward declarations
struct GenomeMode;
class GenomeSystem;

// Color structure for RGB values (packed into 32-bit for GPU efficiency)
struct Color {
    float r, g, b, a;
    
    Color() : r(1.0f), g(1.0f), b(1.0f), a(1.0f) {}
    Color(float red, float green, float blue, float alpha = 1.0f) 
        : r(red), g(green), b(blue), a(alpha) {}
    
    // Pack color into a 32-bit unsigned integer for GPU storage
    uint32_t pack() const {
        uint32_t packed = 0;
        packed |= (uint32_t)(r * 255.0f) << 24;
        packed |= (uint32_t)(g * 255.0f) << 16;
        packed |= (uint32_t)(b * 255.0f) << 8;
        packed |= (uint32_t)(a * 255.0f);
        return packed;
    }
    
    // Unpack color from 32-bit unsigned integer
    static Color unpack(uint32_t packed) {
        return Color(
            ((packed >> 24) & 0xFF) / 255.0f,
            ((packed >> 16) & 0xFF) / 255.0f,
            ((packed >> 8) & 0xFF) / 255.0f,
            (packed & 0xFF) / 255.0f
        );
    }
};

// Genome mode structure - contains all parameters for a specific cell behavior mode
struct GenomeMode {
    // Basic properties
    int index = 0;
    std::string modeName = "Mode 0";
    float splitInterval = 5.0f;          // Time between cell divisions (1-15 seconds)
    bool isInitial = false;              // Whether this is the starting mode
    bool parentMakeAdhesion = false;     // Whether parent creates adhesion bonds
    Color modeColor = Color(1.0f, 1.0f, 1.0f); // Cell color for this mode
      // Parent split settings
    float parentSplitYaw = 0.0f;         // Yaw angle for split direction (-180 to 180)
    float parentSplitPitch = 0.0f;       // Pitch angle for split direction (-90 to 90)
    float parentSplitRoll = 0.0f;        // Roll angle for split direction (-180 to 180)
    
    // Child A settings
    int childAModeIndex = -1;            // Mode index for child A (-1 = use parent mode)
    float childA_OrientationYaw = 0.0f;  // Child A orientation yaw (-180 to 180)
    float childA_OrientationPitch = 0.0f; // Child A orientation pitch (-90 to 90)
    float childA_OrientationRoll = 0.0f; // Child A orientation roll (-180 to 180)
    bool childA_KeepAdhesion = false;    // Whether child A keeps adhesion bonds
    
    // Child B settings
    int childBModeIndex = -1;            // Mode index for child B (-1 = use parent mode)
    float childB_OrientationYaw = 0.0f;  // Child B orientation yaw (-180 to 180)
    float childB_OrientationPitch = 0.0f; // Child B orientation pitch (-90 to 90)
    float childB_OrientationRoll = 0.0f; // Child B orientation roll (-180 to 180)
    bool childB_KeepAdhesion = false;    // Whether child B keeps adhesion bonds
    
    // Adhesion settings
    float adhesionRestLength = 3.0f;     // Natural length of adhesion spring (1-10)
    float adhesionSpringStiffness = 100.0f; // Spring constant (10-500)
    float adhesionSpringDamping = 5.0f;  // Damping coefficient (0-100)
    
    // Orientation constraints
    float orientationConstraintStrength = 0.5f; // Constraint strength (0-1)
    float maxAllowedAngleDeviation = 45.0f;     // Max angle deviation (0-180)
    
    // Adhesion breaking
    bool adhesionCanBreak = false;       // Whether adhesion can break under force
    float adhesionBreakForce = 1000.0f;  // Force threshold for breaking (100-5000)
    
    // Default constructor
    GenomeMode() = default;
    
    // Validation methods
    bool isValid() const;
    void clampValues();
};

// GPU-optimized genome mode data structure for compute shaders
// This structure is tightly packed for efficient GPU memory access
struct GPUGenomeMode {
    // Pack frequently accessed data first for cache efficiency
    uint32_t colorPacked;                    // Packed RGBA color (4 bytes)
    float splitInterval;                     // Time between divisions (4 bytes)
      // Split and orientation data (44 bytes total)
    float parentSplitYaw;                    // Parent split direction yaw
    float parentSplitPitch;                  // Parent split direction pitch
    float parentSplitRoll;                   // Parent split direction roll
    float childA_OrientationYaw;             // Child A orientation yaw
    float childA_OrientationPitch;           // Child A orientation pitch
    float childA_OrientationRoll;            // Child A orientation roll
    float childB_OrientationYaw;             // Child B orientation yaw
    float childB_OrientationPitch;           // Child B orientation pitch
    float childB_OrientationRoll;            // Child B orientation roll
    
    // Adhesion parameters (16 bytes)
    float adhesionRestLength;                // Natural spring length
    float adhesionSpringStiffness;           // Spring stiffness
    float adhesionSpringDamping;             // Damping coefficient
    float adhesionBreakForce;                // Force threshold for breaking
    
    // Orientation constraints (8 bytes)
    float orientationConstraintStrength;     // Constraint strength
    float maxAllowedAngleDeviation;          // Max angle deviation
    
    // Mode indices and flags (8 bytes)
    int childAModeIndex;                     // Child A mode index
    int childBModeIndex;                     // Child B mode index
    
    // Packed boolean flags (4 bytes) - using bit fields for memory efficiency
    uint32_t flags;                          // Packed boolean flags
    
    // Flag bit positions
    static constexpr uint32_t FLAG_IS_INITIAL = 1 << 0;
    static constexpr uint32_t FLAG_PARENT_MAKE_ADHESION = 1 << 1;
    static constexpr uint32_t FLAG_CHILD_A_KEEP_ADHESION = 1 << 2;
    static constexpr uint32_t FLAG_CHILD_B_KEEP_ADHESION = 1 << 3;
    static constexpr uint32_t FLAG_ADHESION_CAN_BREAK = 1 << 4;
    
    // Flag helper methods
    bool getFlag(uint32_t flag) const { return (flags & flag) != 0; }
    void setFlag(uint32_t flag, bool value) {
        if (value) flags |= flag;
        else flags &= ~flag;
    }
    
    // Convert from GenomeMode
    static GPUGenomeMode fromGenomeMode(const GenomeMode& mode);
};

// Event system for genome changes
using GenomeChangeCallback = std::function<void()>;

// Main genome system class - manages all genome modes and provides efficient access
class GenomeSystem {
public:
    // Constructor/Destructor
    GenomeSystem();
    ~GenomeSystem() = default;
    
    // Mode management
    void addMode(const GenomeMode& mode);
    void removeMode(int index);
    void updateMode(int index, const GenomeMode& mode);
    GenomeMode* getMode(int index);
    const GenomeMode* getMode(int index) const;
    
    // Bulk operations
    void clearModes();
    void loadDefaultModes();
    
    // Validation and maintenance
    void refreshModeIndices();
    void enforceSingleInitialMode();
    void validateForSimulation();
    std::vector<int> getInitialModes() const;
    
    // Getters
    size_t getModeCount() const { return modes.size(); }
    int getInitialModeIndex() const;
    const std::vector<GenomeMode>& getModes() const { return modes; }
    
    // GPU data access
    const std::vector<GPUGenomeMode>& getGPUModes() const { return gpuModes; }
    void updateGPUData();
    
    // Event system
    void addChangeCallback(const GenomeChangeCallback& callback);
    void removeChangeCallback(const GenomeChangeCallback& callback);
    void triggerGenomeChanged();
    
    // Serialization (future expansion)
    bool saveToFile(const std::string& filename) const;
    bool loadFromFile(const std::string& filename);
    
private:
    std::vector<GenomeMode> modes;              // CPU-side genome modes
    std::vector<GPUGenomeMode> gpuModes;        // GPU-optimized data
    std::vector<GenomeChangeCallback> callbacks; // Change event callbacks
    
    // Internal helpers
    void updateGPUMode(int index);
    void validateModeIndex(int index) const;
};

// Global genome system instance
extern std::unique_ptr<GenomeSystem> g_genomeSystem;

// Utility functions
namespace GenomeUtils {
    // Angle conversion utilities
    float degreesToRadians(float degrees);
    float radiansToDegrees(float radians);
      // Direction calculation from yaw/pitch
    glm::vec3 getDirectionFromYawPitch(float yaw, float pitch);
    
    // Direction calculation from yaw/pitch/roll
    glm::vec3 getDirectionFromYawPitchRoll(float yaw, float pitch, float roll);
    
    // Full rotation matrix from yaw/pitch/roll
    glm::mat3 getRotationMatrixFromYawPitchRoll(float yaw, float pitch, float roll);
    
    // Color utilities
    Color lerpColor(const Color& a, const Color& b, float t);
    Color randomColor();
    
    // Validation utilities
    bool isValidSplitInterval(float interval);
    bool isValidAngle(float angle, float minAngle, float maxAngle);
    bool isValidModeIndex(int index, int maxModes);
}
