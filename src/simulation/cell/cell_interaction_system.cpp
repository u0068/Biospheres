#include "cell_interaction_system.h"
#include "../../rendering/camera/camera.h"
#include "../../input/input.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

CellInteractionSystem::CellInteractionSystem()
{
    // Initialize interaction system
    selectedCell = SelectedCellInfo{};
    isDraggingCell = false;
}

CellInteractionSystem::~CellInteractionSystem()
{
    // Cleanup if needed
}

void CellInteractionSystem::handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                                            const Camera &camera, bool isMousePressed, bool isMouseDown,
                                            float scrollDelta)
{
    // Handle mouse input for cell selection and interaction
    if (isMousePressed)
    {
        // Calculate mouse ray
        glm::vec3 rayOrigin, rayDirection;
        rayOrigin = camera.getPosition();
        rayDirection = calculateMouseRay(mousePos, screenSize, camera);

        // Try to select a cell
        int selectedIndex = selectCellAtPosition(rayOrigin, rayDirection);
        
        if (selectedIndex >= 0)
        {
            // Cell selected - store selection info
            selectedCell.cellIndex = selectedIndex;
            selectedCell.isValid = true;
            isDraggingCell = true;
            
            // Calculate drag offset and distance
            // This would need access to cell data from the buffer manager
            // For now, we'll set default values
            selectedCell.dragOffset = glm::vec3(0.0f);
            selectedCell.dragDistance = 10.0f;
        }
        else
        {
            // No cell selected - clear selection
            clearSelection();
        }
    }
    else if (isMouseDown && isDraggingCell)
    {
        // Continue dragging
        // This would update the selected cell's position
        // Implementation depends on buffer manager integration
    }
    else if (!isMouseDown && isDraggingCell)
    {
        // End dragging
        endDrag();
    }
}

int CellInteractionSystem::selectCellAtPosition(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection)
{
    // This function would need access to cell data from the buffer manager
    // For now, return -1 (no selection)
    // The actual implementation would:
    // 1. Get cell data from buffer manager
    // 2. Test ray-sphere intersection with each cell
    // 3. Return the index of the closest intersecting cell
    
    return -1; // No cell selected
}

void CellInteractionSystem::dragSelectedCell(const glm::vec3 &newWorldPosition)
{
    if (!selectedCell.isValid) return;

    // Update the selected cell's position
    // This would need access to the buffer manager to update cell data
    // For now, just update the stored cell data
    selectedCell.cellData.positionAndMass = glm::vec4(newWorldPosition, selectedCell.cellData.positionAndMass.w);
}

void CellInteractionSystem::clearSelection()
{
    selectedCell = SelectedCellInfo{};
    isDraggingCell = false;
}

void CellInteractionSystem::endDrag()
{
    if (!isDraggingCell) return;

    // Restore physics for the dragged cell
    // This would need access to the buffer manager to update cell data
    isDraggingCell = false;
}

glm::vec3 CellInteractionSystem::calculateMouseRay(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                                                   const Camera &camera)
{
    // Convert mouse position to normalized device coordinates
    float x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * mousePos.y) / screenSize.y;
    
    // Create clip space coordinates
    glm::vec4 clipCoords = glm::vec4(x, y, -1.0f, 1.0f);
    
    // Transform to eye space
    glm::mat4 projection = camera.getProjectionMatrix();
    glm::vec4 eyeCoords = glm::inverse(projection) * clipCoords;
    eyeCoords = glm::vec4(eyeCoords.x, eyeCoords.y, -1.0f, 0.0f);
    
    // Transform to world space
    glm::mat4 view = camera.getViewMatrix();
    glm::vec4 worldCoords = glm::inverse(view) * eyeCoords;
    
    // Normalize the ray direction
    glm::vec3 rayDirection = glm::normalize(glm::vec3(worldCoords));
    
    return rayDirection;
}

bool CellInteractionSystem::raySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                                                  const glm::vec3 &sphereCenter, float sphereRadius, float &distance)
{
    // Calculate ray-sphere intersection
    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(oc, rayDirection);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0)
    {
        return false; // No intersection
    }
    
    // Calculate intersection distance
    float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
    float t2 = (-b + sqrt(discriminant)) / (2.0f * a);
    
    // Return the closest positive intersection
    if (t1 > 0)
    {
        distance = t1;
        return true;
    }
    else if (t2 > 0)
    {
        distance = t2;
        return true;
    }
    
    return false; // Intersection behind ray origin
} 