#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "glad/glad.h"
#include <glm/gtc/quaternion.hpp>

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
    glm::vec4 signallingSubstances{}; // 4 substances for now
    int modeIndex{ 0 };
    float age{ 0 };                      // also used for split timer
    float toxins{ 0 };
    float nitrates{ 1 };

    float getRadius() const
    {
        return static_cast<float>(pow(positionAndMass.w, 1.0f / 3.0f));
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
    float maxAngularDeviation = 45.0f; // degrees
};

struct GPUMode {
    glm::vec4 color{ 1.};  // R, G, B padding
    glm::quat orientationA{1., 0., 0., 0.};  // quaternion
    glm::quat orientationB{1., 0., 0., 0.};  // quaternion
    glm::vec4 splitDirection{ 1., 0. ,0. ,0. };// x, y, z, padding
    glm::ivec2 childModes{ 0 };
    float splitInterval{ 5. };
    int genomeOffset{ 0 };  // Offset into global buffer where this genome starts
	AdhesionSettings adhesionSettings; // Adhesion settings for the parent cell
    bool parentMakeAdhesion{ 0 };  // Boolean flag for adhesion creation (0 = false, 1 = true) + padding
	int padding[3]{ 0 }; // Padding to ensure 16-byte alignment for GPU compatibility
};

struct AdhesionConnection
{
    int cellIndexA; // Index of the first cell in the connection
    int cellIndexB; // Index of the second cell in the connection
	int modeIndex;  // Mode index for the connection (to look up adhesion settings)
	bool isActive; // Whether the connection is currently active
};

struct ChildSettings
{
    int modeNumber = 0;
    glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f); // quaternion, identity by default
    bool keepAdhesion = false;
};

struct ModeSettings
{
    std::string name = "Untitled Mode";
    glm::vec3 color = { 1.0f, 1.0f, 1.0f }; // RGB color    // Parent Settings
    bool parentMakeAdhesion = true;
    float splitMass = 1.0f;
    float splitInterval = 5.0f;
    glm::vec2 parentSplitDirection = { 0.0f, 0.0f}; // pitch, yaw in degrees

    // Child Settings
    ChildSettings childA;
    ChildSettings childB;

    // Adhesion Settings
    AdhesionSettings adhesion;
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