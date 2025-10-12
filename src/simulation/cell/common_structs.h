#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "glad/glad.h"
#include <glm/gtc/quaternion.hpp>

#include "../../core/config.h"

// GPU compute cell structure matching the compute shader
struct alignas(16) ComputeCell {
    // Physics:
    glm::vec4 positionAndMass{ 0, 0, 0, 1 };   // x, y, z, mass
    glm::vec4 velocity{};                               // x, y, z, padding
    glm::vec4 acceleration{};                           // x, y, z, padding
    glm::vec4 prevAcceleration{};                       // x, y, z, padding
    glm::vec4 orientation{ 1., 0., 0., 0. };   // Quaternion to prevent gimbal lock
    glm::vec4 angularVelocity{};                        // yz, zx, xy, padding
    glm::vec4 angularAcceleration{};                    // yz, zx, xy, padding
    glm::vec4 prevAngularAcceleration{};                // yz, zx, xy, padding

    // Internal:
    glm::vec4 signallingSubstances{};   // 4 substances for now
    int modeIndex{ 0 };
    float age{ 0 };                     // also used for split timer
    float toxins{ 0 };
    float nitrates{ 1 };
    int adhesionIndices[20]{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ,-1, -1, -1, -1, -1 };
    
    // Padding to maintain 16-byte alignment for the entire struct
    uint32_t _padding[8]{ 0 };

    
    float getRadius() const
    {
        return pow(positionAndMass.w, 1.0f / 3.0f);
    }
    
    };

struct alignas(4) AdhesionSettings
{
    bool canBreak = true;
    float breakForce = 10.0f;
    float restLength = 1.0f;
    float linearSpringStiffness = 150.0f;
    float linearSpringDamping = 5.0f;
    float orientationSpringStiffness = 10.0f;
    float orientationSpringDamping = 2.0f;
    float maxAngularDeviation = 0.0f; // degrees - 0 = strict orientation locking, >0 = flexible with max deviation
    
    // Twist constraint parameters
    float twistConstraintStiffness = 2.0f; // Stiffness of twist constraint around adhesion axis (increased for snake body alignment)
    float twistConstraintDamping = 0.5f;   // Damping of twist constraint (high for stable snake body)
    bool enableTwistConstraint = true;     // Whether to apply twist constraints
    
    // Padding to ensure proper alignment - compiler will add as needed
    char padding[1] = {0}; // Reduced padding, let compiler handle the rest
};

// GPU-side mirror of AdhesionSettings with std430-compatible packing (no bools)
struct alignas(16) GPUModeAdhesionSettings
{
    int canBreak = 1;                 // bool -> int
    float breakForce = 10.0f;
    float restLength = 1.0f;
    float linearSpringStiffness = 150.0f;
    float linearSpringDamping = 5.0f;
    float orientationSpringStiffness = 10.0f;
    float orientationSpringDamping = 2.0f;
    float maxAngularDeviation = 0.0f;
    float twistConstraintStiffness = 2.0f;
    float twistConstraintDamping = 0.5f;
    int enableTwistConstraint = 1;    // bool -> int
    int _padding = 0;                 // pad to 48 bytes
};
static_assert(sizeof(GPUModeAdhesionSettings) % 16 == 0, "GPUModeAdhesionSettings must be 16-byte aligned for GPU usage");

struct alignas(16) GPUMode {
    glm::vec4 color{ 1.};  // 16 bytes
    glm::quat orientationA{1., 0., 0., 0.};  // 16 bytes
    glm::quat orientationB{1., 0., 0., 0.};  // 16 bytes
    glm::vec4 splitDirection{ 1., 0. ,0. ,0. };// 16 bytes
    glm::ivec2 childModes{ 0 };  // 8 bytes
    float splitInterval{ 5. };   // 4 bytes
    int genomeOffset{ 0 };       // 4 bytes (total 16 bytes for this line)
	GPUModeAdhesionSettings adhesionSettings{}; // 48 bytes
    int parentMakeAdhesion{ 0 };  // 4 bytes
    int childAKeepAdhesion{ 1 };  // 4 bytes
	int childBKeepAdhesion{ 1 };  // 4 bytes
	int maxAdhesions{ config::MAX_ADHESIONS_PER_CELL }; // 4 bytes (total 16 bytes)
    float flagellocyteThrustForce{ 0.0f }; // 4 bytes
    float _padding1{ 0.0f };      // 4 bytes
    float _padding2{ 0.0f };      // 4 bytes
    float _padding3{ 0.0f };      // 4 bytes (total 16 bytes)
};

struct AdhesionConnection
{
    uint32_t cellAIndex; // Index of the first cell in the connection
    uint32_t cellBIndex; // Index of the second cell in the connection
    uint32_t modeIndex;  // Mode index for the connection (to look up adhesion settings)
    uint32_t isActive;   // Whether the connection is currently active (1 = active, 0 = inactive)
    uint32_t zoneA;      // Zone classification for cell A (0=ZoneA, 1=ZoneB, 2=ZoneC)
    uint32_t zoneB;      // Zone classification for cell B (0=ZoneA, 1=ZoneB, 2=ZoneC)
    glm::vec3 anchorDirectionA; // Anchor direction for cell A in local cell space (normalized)
    float paddingA; // Padding to ensure 16-byte alignment
    glm::vec3 anchorDirectionB; // Anchor direction for cell B in local cell space (normalized)
    float paddingB; // Padding to ensure 16-byte alignment
    glm::quat twistReferenceA; // Reference quaternion for twist constraint for cell A (16 bytes)
    glm::quat twistReferenceB; // Reference quaternion for twist constraint for cell B (16 bytes)
    uint32_t _padding[2]; // Padding to ensure 16-byte alignment (96 bytes total)
};
static_assert(sizeof(AdhesionConnection) == 96, "AdhesionConnection must be exactly 96 bytes (6 uints + 2 vec3s + 2 floats + 2 quats + 2 uint padding)");
static_assert(sizeof(AdhesionConnection) % 16 == 0, "AdhesionConnection must be 16-byte aligned for GPU usage");

// Anchor instance data for gizmo rendering (matches GPU AnchorInstance structure)
// Must be 16-byte aligned and total 48 bytes to match shader
struct AnchorInstance
{
    glm::vec4 positionAndRadius;  // xyz = position, w = radius (16 bytes)
    glm::vec4 color;              // rgb = color, a = unused (16 bytes)  
    glm::vec4 orientation;        // quaternion (unused for spheres, but matches structure) (16 bytes)
};
static_assert(sizeof(AnchorInstance) == 48, "AnchorInstance must be 48 bytes for GPU compatibility");
static_assert(sizeof(AnchorInstance) % 16 == 0, "AnchorInstance must be 16-byte aligned for GPU usage");

enum class CellType : uint8_t
{
    Phagocyte = 0,
    Flagellocyte = 1,
    COUNT
};

// Helper function to get cell type name
inline const char* getCellTypeName(CellType type)
{
    switch (type)
    {
        case CellType::Phagocyte: return "Phagocyte";
        case CellType::Flagellocyte: return "Flagellocyte";
        default: return "Unknown";
    }
}

    // Flagellocyte tail settings
    struct FlagellocyteSettings
    {
        float tailLength = 5.0f;          // Length of the spiral tail
        float tailThickness = 0.15f;      // Thickness of the tail
        float spiralTightness = 2.0f;     // Number of complete spirals per unit length
        float spiralRadius = 0.3f;        // Radius of the spiral
        float rotationSpeed = 2.0f;       // Rotation speed in radians per second
        float tailTaper = 1.0f;           // Amount of taper from base to tip (0=no taper, 1=full taper to point)
        int segments = 32;                // Number of segments in the tail
        glm::vec3 tailColor = { 0.8f, 0.9f, 1.0f }; // Tail color (can differ from body)
        float thrustForce = 5.0f;         // Forward thrust force applied continuously
    };

struct ChildSettings
{
    int modeNumber = 0;
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // quaternion, identity by default
    bool keepAdhesion = true;
};

struct ModeSettings
{
    std::string name = "Untitled Mode";
    CellType cellType = CellType::Phagocyte; // Cell type for this mode
    glm::vec3 color = { 1.0f, 1.0f, 1.0f }; // RGB color    // Parent Settings
    bool parentMakeAdhesion = false;
    float splitMass = 1.0f;
    float splitInterval = 5.0f;
    glm::vec2 parentSplitDirection = { 0.0f, 0.0f}; // pitch, yaw in degrees
    int maxAdhesions{ config::MAX_ADHESIONS_PER_CELL }; // Maximum number of adhesions for this mode

    // Child Settings
    ChildSettings childA;
    ChildSettings childB;

    // Adhesion Settings
    AdhesionSettings adhesionSettings;
    
    // Flagellocyte Settings (only used when cellType == Flagellocyte)
    FlagellocyteSettings flagellocyteSettings;
};

struct GenomeData
{
    std::string name = "Untitled Genome";
    int initialMode = 0;
    glm::quat initialOrientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // Separate orientation for initial cell
    std::vector<ModeSettings> modes;

    GenomeData()
    {
        // Initialize with one default mode
        modes.push_back(ModeSettings());
        modes[0].name = "Default Mode";
    }
};