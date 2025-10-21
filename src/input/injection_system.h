#pragma once

#include <glm/glm.hpp>
#include <string>

// Forward declarations
class SpatialGridSystem;
class Camera;

enum class InjectionMode {
    CellSelection = 1,  // Key 1
    Density = 2,        // Key 2
    Velocity = 3        // Key 3
};

class InjectionSystem {
private:
    InjectionMode currentMode{InjectionMode::CellSelection};
    
    // Injection Parameters
    float injectionRadius{2.0f};
    float injectionStrength{1.0f};
    glm::vec3 velocityDirection{0.0f, 1.0f, 0.0f};
    
    // Injection Plane
    float injectionPlaneDistance{0.0f}; // Distance from camera
    glm::vec3 injectionPlaneNormal;     // Camera forward vector
    
    // Translucent Brush (Follows Mouse Cursor)
    glm::vec3 brushPosition;        // Updated continuously with mouse movement
    bool brushVisible{false};
    bool isInjecting{false};        // Track if currently injecting
    glm::vec2 lastMousePosition;    // Track mouse for continuous updates
    
public:
    // Mode Management
    void setMode(InjectionMode mode);
    InjectionMode getCurrentMode() const { return currentMode; }
    
    // Input Handling
    void handleKeyInput(int key);
    void handleMouseClick(const glm::vec2& screenPos, const Camera& camera, SpatialGridSystem& spatialGrid, const glm::vec2& screenSize = glm::vec2(1280, 720));
    void handleMouseDrag(const glm::vec2& screenPos, const Camera& camera, SpatialGridSystem& spatialGrid, const glm::vec2& screenSize = glm::vec2(1280, 720));
    void handleMouseMove(const glm::vec2& screenPos, const Camera& camera, const glm::vec2& screenSize = glm::vec2(1280, 720)); // Continuous brush updates
    void handleScrollWheel(float delta);
    
    // Brush Management (Mouse Following)
    void updateBrushPosition(const glm::vec2& mousePos, const Camera& camera, const glm::vec2& screenSize = glm::vec2(1280, 720));
    glm::vec3 projectMouseToInjectionPlane(const glm::vec2& mousePos, const Camera& camera, const glm::vec2& screenSize);
    glm::vec3 calculateMouseRay(const glm::vec2& mousePos, const glm::vec2& screenSize, const Camera& camera);
    glm::vec3 getBrushPosition() const { return brushPosition; }
    bool isBrushVisible() const { return brushVisible; }
    bool isCurrentlyInjecting() const { return isInjecting; }
    
    // Injection Operations (Updated to work with SpatialGridSystem)
    void performInjection(SpatialGridSystem& spatialGrid, const glm::vec3& worldPos);
    
    // Parameter Getters/Setters
    float getInjectionRadius() const { return injectionRadius; }
    void setInjectionRadius(float radius) { injectionRadius = radius; }
    
    float getInjectionStrength() const { return injectionStrength; }
    void setInjectionStrength(float strength) { injectionStrength = strength; }
    
    glm::vec3 getVelocityDirection() const { return velocityDirection; }
    void setVelocityDirection(const glm::vec3& direction) { velocityDirection = direction; }
    
    float getInjectionPlaneDistance() const { return injectionPlaneDistance; }
    void setInjectionPlaneDistance(float distance) { injectionPlaneDistance = distance; }
    
    // Visual feedback for current distance (Requirement 7.5)
    std::string getCurrentDistanceInfo() const;
};