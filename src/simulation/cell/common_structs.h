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
    glm::vec4 positionAndMass{ 0, 0, 0, 1 };       // x, y, z, mass
    glm::vec4 velocity{};
    glm::vec4 acceleration{};
    glm::quat orientation{ 1., 0., 0., 0. };  // angular stuff in quaternions to prevent gimbal lock
    glm::quat angularVelocity{ 1., 0., 0., 0. };
    glm::quat angularAcceleration{ 1., 0., 0., 0. };

    // Internal:
    glm::vec4 signallingSubstances{};   // 4 substances for now
    int modeIndex{ 0 };
    float age{ 0 };                     // also used for split timer
    float toxins{ 0 };
    float nitrates{ 1 };
    int adhesionIndices[20]{ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 ,-1, -1, -1, -1, -1 };

    float getRadius() const
    {
        return pow(positionAndMass.w, 1.0f / 3.0f);
    }
};

struct AdhesionSettings
{
    bool canBreak = true;
    float breakForce = 10.0f;
    float restLength = 2.0f;
    float linearSpringStiffness = 5.0f;
    float linearSpringDamping = 0.5f;
    float orientationSpringStiffness = 2.0f;
    float orientationSpringDamping = 0.5f;
    float maxAngularDeviation = 0.0f; // degrees - 0 = strict orientation locking, >0 = flexible with max deviation
};

struct GPUMode {
    glm::vec4 color{ 1.};  // R, G, B padding
    glm::quat orientationA{1., 0., 0., 0.};  // quaternion
    glm::quat orientationB{1., 0., 0., 0.};  // quaternion
    glm::vec4 splitDirection{ 1., 0. ,0. ,0. };// x, y, z, padding
    glm::ivec2 childModes{ 0 };
    float splitInterval{ 5. };
    int genomeOffset{ 0 };  // Offset into global buffer where this genome starts
	AdhesionSettings adhesionSettings{}; // Adhesion settings for the parent cell
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
};

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