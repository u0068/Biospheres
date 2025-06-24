#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include "glad/glad.h"
#include <glm/gtc/quaternion.hpp>

// Genome Editor Data Structures
struct AdhesionSettings
{
    float breakForce{10.0f};
    float restLength{2.0f};
    float linearSpringStiffness{5.0f};
    float linearSpringDamping{0.5f};
    float angularSpringStiffness{2.0f};
    float maxAngularDeviation{45.0f}; // degrees
    bool enabled{ false };
    bool canBreak{ true };
    float padding[4]{};   // Structs need to align to 16 bytes in glsl
};

struct GPUMode {
    glm::vec4 color{ 1.};  // R, G, B padding
    glm::quat orientationA{1., 0., 0., 0.};  // quaternion
    glm::quat orientationB{1., 0., 0., 0.};  // quaternion
    glm::vec4 splitDirection{ 1., 0. ,0. ,0. };// x, y, z, padding
    glm::ivec2 childModes{ 0 };
    float splitInterval{ 5. };
    int genomeOffset{ 0 };  // Offset into global buffer where this genome starts
    AdhesionSettings adhesionSettings{};
};

struct Adhesion
{
    glm::quat restOrientationOffset{ 1., 0., 0., 0. };
    glm::ivec2 connectedCells{ -1 };
    int modeIndex{ 0 }; // To get the mode's adhesion settings
    float padding{ 0 };
};

struct ChildSettings
{
    int modeNumber = 0;
    glm::vec3 orientation = { 0.0f, 0.0f, 0.0f }; // pitch, yaw, roll in degrees
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
    std::vector<ModeSettings> modes;

    GenomeData()
    {
        // Initialize with one default mode
        modes.push_back(ModeSettings());
        modes[0].name = "Default Mode";
    }
};