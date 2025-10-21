#include "injection_system.h"
#include "../simulation/spatial/spatial_grid_system.h"
#include "../rendering/camera/camera.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

void InjectionSystem::setMode(InjectionMode mode) {
    currentMode = mode;
    
    // Update brush visibility based on mode
    if (mode == InjectionMode::Density || mode == InjectionMode::Velocity) {
        brushVisible = true;
    } else {
        brushVisible = false;
    }
}

void InjectionSystem::handleKeyInput(int key) {
    // Requirement 5.1: Use key "1" for cell selection and movement mode
    // Requirement 5.2: Use key "2" for density injection mode
    // Requirement 5.3: Use key "3" for velocity injection mode
    
    switch (key) {
        case GLFW_KEY_1:
            setMode(InjectionMode::CellSelection);
            break;
        case GLFW_KEY_2:
            setMode(InjectionMode::Density);
            break;
        case GLFW_KEY_3:
            setMode(InjectionMode::Velocity);
            break;
    }
}

void InjectionSystem::handleMouseClick(const glm::vec2& screenPos, const Camera& camera, SpatialGridSystem& spatialGrid, const glm::vec2& screenSize) {
    // Only perform injection in density or velocity modes
    if (currentMode == InjectionMode::Density || currentMode == InjectionMode::Velocity) {
        // Update brush position and perform injection
        updateBrushPosition(screenPos, camera, screenSize);
        performInjection(spatialGrid, brushPosition);
        isInjecting = true;
    }
}

void InjectionSystem::handleMouseDrag(const glm::vec2& screenPos, const Camera& camera, SpatialGridSystem& spatialGrid, const glm::vec2& screenSize) {
    // Only perform injection in density or velocity modes
    if (currentMode == InjectionMode::Density || currentMode == InjectionMode::Velocity) {
        // Update brush position and perform injection
        updateBrushPosition(screenPos, camera, screenSize);
        performInjection(spatialGrid, brushPosition);
        isInjecting = true;
    }
}

void InjectionSystem::handleMouseMove(const glm::vec2& screenPos, const Camera& camera, const glm::vec2& screenSize) {
    // Requirement 5.4: Project Translucent_Brush onto Injection_Plane at configurable distance from camera
    
    lastMousePosition = screenPos;
    
    if (currentMode == InjectionMode::Density || currentMode == InjectionMode::Velocity) {
        updateBrushPosition(screenPos, camera, screenSize);
        brushVisible = true;
    } else {
        brushVisible = false; // Hide brush in cell selection mode
        isInjecting = false;   // Reset injection state when not in injection mode
    }
    
    // Reset injection state on mouse move (injection only happens on click/drag)
    isInjecting = false;
}

void InjectionSystem::handleScrollWheel(float delta) {
    // Requirement 7.3: Use scroll wheel for Injection_Plane distance adjustment in Key 2 and Key 3 modes
    // Requirement 7.2: Preserve existing scroll wheel cell distance adjustment in Key 1 mode
    
    if (currentMode == InjectionMode::Density || currentMode == InjectionMode::Velocity) {
        // Adjust injection plane distance (forward/backward from camera)
        float adjustmentSpeed = 2.0f; // Adjust this for sensitivity
        injectionPlaneDistance += delta * adjustmentSpeed;
        
        // Clamp to reasonable bounds
        injectionPlaneDistance = std::clamp(injectionPlaneDistance, -50.0f, 50.0f);
        

    }
    // In CellSelection mode, scroll wheel is handled by camera system (no changes needed)
}

void InjectionSystem::updateBrushPosition(const glm::vec2& mousePos, const Camera& camera, const glm::vec2& screenSize) {
    brushPosition = projectMouseToInjectionPlane(mousePos, camera, screenSize);
}

glm::vec3 InjectionSystem::projectMouseToInjectionPlane(const glm::vec2& mousePos, const Camera& camera, const glm::vec2& screenSize) {
    // Requirement 6.1: Create brush position calculation using mouse-to-world projection
    // Ray projects from camera center through mouse position to injection plane
    
    // Ray origin is the camera position (center of camera)
    glm::vec3 rayOrigin = camera.getPosition();
    
    // Ray direction goes from camera center toward mouse position in world space
    glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
    
    // Define injection plane perpendicular to camera forward at specified distance
    glm::vec3 planeNormal = camera.getFront();
    float planeDistance = 15.0f + injectionPlaneDistance;
    glm::vec3 planePoint = rayOrigin + planeNormal * planeDistance;
    
    // Calculate proper ray-plane intersection
    // Ray equation: P = rayOrigin + t * rayDirection
    // Plane equation: dot(P - planePoint, planeNormal) = 0
    // Solving for t: t = dot(planePoint - rayOrigin, planeNormal) / dot(rayDirection, planeNormal)
    
    float denominator = glm::dot(rayDirection, planeNormal);
    if (abs(denominator) > 0.0001f) {
        float t = glm::dot(planePoint - rayOrigin, planeNormal) / denominator;
        if (t >= 0) {
            // This is the exact point where the mouse ray intersects the injection plane
            return rayOrigin + rayDirection * t;
        }
    }
    
    // Fallback to plane center if intersection fails
    return planePoint;
}

glm::vec3 InjectionSystem::calculateMouseRay(const glm::vec2& mousePos, const glm::vec2& screenSize, const Camera& camera) {
    // Same implementation as CellManager::calculateMouseRay for consistency
    
    // Safety check for zero screen size
    if (screenSize.x <= 0 || screenSize.y <= 0) {
        return camera.getFront();
    }
    
    // Convert screen coordinates to normalized device coordinates (-1 to 1)
    float x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * mousePos.y) / screenSize.y;
    
    // Create projection matrix (matching the one used in rendering)
    float aspectRatio = screenSize.x / screenSize.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
        return camera.getFront();
    }
    
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 viewProjection = projection * view;
    
    // Check if the matrix is invertible
    float determinant = glm::determinant(viewProjection);
    if (abs(determinant) < 1e-6f) {
        return camera.getFront();
    }
    
    glm::mat4 inverseVP = glm::inverse(viewProjection);
    
    // Create normalized device coordinate points for near and far planes
    glm::vec4 rayClipNear = glm::vec4(x, y, -1.0f, 1.0f);
    glm::vec4 rayClipFar = glm::vec4(x, y, 1.0f, 1.0f);
    
    // Transform to world space
    glm::vec4 rayWorldNear = inverseVP * rayClipNear;
    glm::vec4 rayWorldFar = inverseVP * rayClipFar;
    
    // Convert from homogeneous coordinates with safety checks
    if (abs(rayWorldNear.w) < 1e-6f || abs(rayWorldFar.w) < 1e-6f) {
        return camera.getFront();
    }
    
    rayWorldNear /= rayWorldNear.w;
    rayWorldFar /= rayWorldFar.w;
    
    // Calculate ray direction
    glm::vec3 rayDirection = glm::vec3(rayWorldFar) - glm::vec3(rayWorldNear);
    
    // Check if the direction is valid and normalize
    if (glm::length(rayDirection) < 1e-6f) {
        return camera.getFront();
    }
    
    rayDirection = glm::normalize(rayDirection);
    
    // Final validation
    if (!std::isfinite(rayDirection.x) || !std::isfinite(rayDirection.y) || !std::isfinite(rayDirection.z)) {
        return camera.getFront();
    }
    
    return rayDirection;
}

void InjectionSystem::performInjection(SpatialGridSystem& spatialGrid, const glm::vec3& worldPos) {
    // Requirement 5.1, 5.2, 5.3: Route injection to appropriate spatial grid methods based on mode
    
    switch (currentMode) {
        case InjectionMode::CellSelection:
            // No injection in cell selection mode
            break;
            
        case InjectionMode::Density:
            // Requirement 5.2: Density injection mode
            spatialGrid.injectDensity(worldPos, injectionRadius, injectionStrength);
            break;
            
        case InjectionMode::Velocity:
            // Requirement 5.3: Velocity injection mode

            spatialGrid.injectVelocity(worldPos, injectionRadius, velocityDirection, injectionStrength);
            break;
    }
}



std::string InjectionSystem::getCurrentDistanceInfo() const {
    // Requirement 7.5: Add visual feedback for current distance (cell or injection plane)
    
    switch (currentMode) {
        case InjectionMode::CellSelection:
            return "Mode: Cell Selection (scroll adjusts cell distance)";
        case InjectionMode::Density:
            return "Mode: Density Injection | Plane Distance: " + std::to_string(injectionPlaneDistance);
        case InjectionMode::Velocity:
            return "Mode: Velocity Injection | Plane Distance: " + std::to_string(injectionPlaneDistance);
        default:
            return "Mode: Unknown";
    }
}

