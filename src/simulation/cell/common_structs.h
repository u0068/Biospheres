#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "glad/glad.h"
#include <glm/gtc/quaternion.hpp>

#include "../../core/config.h"

// GPU compute cell structure matching the compute shader
struct ComputeCell {
    // Physics:
    glm::vec4 positionAndMass{ 0, 0, 0, 1 };   // x, y, z, mass
    glm::vec4 velocity{};                               // x, y, z, padding
    glm::vec4 acceleration{};                           // x, y, z, padding
    glm::vec4 prevAcceleration{};                       // x, y, z, padding
    glm::quat orientation{ 1., 0., 0., 0. };   // Quaternion to prevent gimbal lock
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

    // Lineage tracking (AA.BB.C format)
    uint32_t parentLineageId{ 0 };      // AA: Parent's unique ID (0 for root cells)
    uint32_t uniqueId{ 0 };             // BB: This cell's unique ID
    uint32_t childNumber{ 0 };          // C: Child number (1 or 2, 0 for root cells)
    uint32_t _lineagePadding{ 0 };      // Padding to maintain 16-byte alignment

    float getRadius() const
    {
        return pow(positionAndMass.w, 1.0f / 3.0f);
    }
    
    // Generate lineage string in A.B.C format where:
    // A = parent unique ID, B = cell unique ID, C = child number (1 or 2)
    std::string getLineageString() const
    {
        if (parentLineageId == 0) {
            // Root cell: parent ID=0, cell unique ID, child number=0
            return std::to_string(0) + "." +
                   std::to_string(uniqueId) + "." +
                   std::to_string(0);
        }
        return std::to_string(parentLineageId) + "." +
               std::to_string(uniqueId) + "." +
               std::to_string(childNumber);
    }
};

struct alignas(4) AdhesionSettings
{
    bool canBreak = true;
    float breakForce = 10.0f;
    float restLength = 2.0f;
    float linearSpringStiffness = 5.0f;
    float linearSpringDamping = 0.5f;
    float orientationSpringStiffness = 2.0f;
    float orientationSpringDamping = 0.5f;
    float maxAngularDeviation = 0.0f; // degrees - 0 = strict orientation locking, >0 = flexible with max deviation
    
    // Twist constraint parameters
    float twistConstraintStiffness = 0.5f; // Stiffness of twist constraint around adhesion axis (increased for snake body alignment)
    float twistConstraintDamping = 0.8f;   // Damping of twist constraint (high for stable snake body)
    bool enableTwistConstraint = true;     // Whether to apply twist constraints
    
    // Padding to ensure proper alignment - compiler will add as needed
    char padding[1] = {0}; // Reduced padding, let compiler handle the rest
};

// GPU-side mirror of AdhesionSettings with std430-compatible packing (no bools)
struct alignas(16) GPUModeAdhesionSettings
{
    int canBreak = 1;                 // bool -> int
    float breakForce = 10.0f;
    float restLength = 2.0f;
    float linearSpringStiffness = 5.0f;
    float linearSpringDamping = 0.5f;
    float orientationSpringStiffness = 2.0f;
    float orientationSpringDamping = 0.5f;
    float maxAngularDeviation = 0.0f;
    float twistConstraintStiffness = 0.5f;
    float twistConstraintDamping = 0.8f;
    int enableTwistConstraint = 1;    // bool -> int
    int _padding = 0;                 // pad to 48 bytes
};
static_assert(sizeof(GPUModeAdhesionSettings) % 16 == 0, "GPUModeAdhesionSettings must be 16-byte aligned for GPU usage");

struct alignas(16) GPUMode {
    glm::vec4 color{ 1.};  // R, G, B padding
    glm::quat orientationA{1., 0., 0., 0.};  // quaternion
    glm::quat orientationB{1., 0., 0., 0.};  // quaternion
    glm::vec4 splitDirection{ 1., 0. ,0. ,0. };// x, y, z, padding
    glm::ivec2 childModes{ 0 };
    float splitInterval{ 5. };
    int genomeOffset{ 0 };  // Offset into global buffer where this genome starts
	GPUModeAdhesionSettings adhesionSettings{}; // Packed GPU adhesion settings
    int parentMakeAdhesion{ 0 };  // Boolean flag for adhesionSettings creation (0 = false, 1 = true) + padding
    int childAKeepAdhesion{ 1 };
	int childBKeepAdhesion{ 1 };
	int maxAdhesions{ config::MAX_ADHESIONS_PER_CELL }; // Maximum number of adhesions for this mode
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

struct ChildSettings
{
    int modeNumber = 0;
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // quaternion, identity by default
    bool keepAdhesion = true;
};

struct ModeSettings
{
    std::string name = "Untitled Mode";
    glm::vec3 color = { 1.0f, 1.0f, 1.0f }; // RGB color    // Parent Settings
    bool parentMakeAdhesion = true;
    float splitMass = 1.0f;
    float splitInterval = 5.0f;
    glm::vec2 parentSplitDirection = { 0.0f, 0.0f}; // pitch, yaw in degrees
    int maxAdhesions{ config::MAX_ADHESIONS_PER_CELL }; // Maximum number of adhesions for this mode

    // Child Settings
    ChildSettings childA;
    ChildSettings childB;

    // Adhesion Settings
    AdhesionSettings adhesionSettings;
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