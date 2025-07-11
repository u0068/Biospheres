#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../cell/common_structs.h"

// Forward declaration
class Camera;

struct CellInteractionManager
{
    // Cell selection and interaction system
    struct SelectedCellInfo
    {
        int cellIndex = -1;
        ComputeCell cellData;
        bool isValid = false;
        glm::vec3 dragOffset = glm::vec3(0.0f); // Offset from cell center when dragging starts
        float dragDistance = 10.0f;             // Distance from camera to maintain during dragging
    };

    SelectedCellInfo selectedCell;
    bool isDraggingCell = false;

    // Constructor and destructor
    CellInteractionManager();
    ~CellInteractionManager();

    // Selection and interaction functions
    void handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                          const Camera &camera, bool isMousePressed, bool isMouseDown,
                          float scrollDelta = 0.0f);
    int selectCellAtPosition(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection);
    void dragSelectedCell(const glm::vec3 &newWorldPosition);
    void clearSelection();
    void endDrag();

    // Utility functions for mouse interaction
    glm::vec3 calculateMouseRay(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                                const Camera &camera);
    bool raySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                               const glm::vec3 &sphereCenter, float sphereRadius, float &distance);

    // Getters for selection system
    bool hasSelectedCell() const { return selectedCell.isValid; }
    const SelectedCellInfo &getSelectedCell() const { return selectedCell; }
}; 