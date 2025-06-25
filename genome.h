#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "glad/glad.h"
#include <glm/gtc/quaternion.hpp>

struct GPUMode {
    glm::vec4 color{ 1.};  // R, G, B padding
    glm::quat orientationA{1., 0., 0., 0.};  // quaternion
    glm::quat orientationB{1., 0., 0., 0.};  // quaternion
    glm::vec4 splitDirection{ 1., 0. ,0. ,0. };// x, y, z, padding
    glm::ivec2 childModes{ 0 };
    float splitInterval{ 5. };
    int genomeOffset{ 0 };  // Offset into global buffer where this genome starts
};

// Genome Editor Data Structures
struct AdhesionSettings
{
    bool canBreak = true;
    float breakForce = 10.0f;
    float restLength = 2.0f;
    float linearSpringStiffness = 5.0f;
    float linearSpringDamping = 0.5f;
    float orientationSpringStrength = 2.0f;
    float maxAngularDeviation = 45.0f; // degrees
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