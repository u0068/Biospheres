#pragma once
#include <string>
#include <vector>
#include <glm/glm.hpp>

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
    glm::vec3 parentSplitOrientation = { 0.0f, 0.0f, 0.0f }; // pitch, yaw, roll in degrees

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