#include "cell_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../core/config.h"
#include "../../ui/ui_manager.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <vector>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <GLFW/glfw3.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "../../utils/timer.h"

void CellManager::handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
    const Camera &camera, bool isMousePressed, bool isMouseDown,
    float scrollDelta)
{
// Safety check for invalid screen size (minimized window)
if (screenSize.x <= 0 || screenSize.y <= 0)
{
return;
}

// Handle scroll wheel to adjust drag distance when cell is selected
if (selectedCell.isValid && scrollDelta != 0.0f)
{
float scrollSensitivity = 2.0f;
selectedCell.dragDistance += scrollDelta * scrollSensitivity;
selectedCell.dragDistance = glm::clamp(selectedCell.dragDistance, 1.0f, 100.0f);

// If we're dragging, update the cell position to maintain the new distance
if (isDraggingCell)
{
glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
glm::vec3 newWorldPos = camera.getPosition() + rayDirection * selectedCell.dragDistance;
dragSelectedCell(newWorldPos);
}
}
if (isMousePressed && !isDraggingCell)
{
// Sync current cell positions from GPU before attempting selection
syncCellPositionsFromGPU();

// Start new selection with improved raycasting
glm::vec3 rayOrigin = camera.getPosition();
glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
// Debug: Print mouse coordinates and ray info (reduced logging)
std::cout << "Mouse click at (" << mousePos.x << ", " << mousePos.y << ")\n";

int selectedIndex = selectCellAtPosition(rayOrigin, rayDirection);
if (selectedIndex >= 0)
{
selectedCell.cellIndex = selectedIndex;
selectedCell.cellData = cpuCells[selectedIndex];
selectedCell.isValid = true;

// Calculate the distance from camera to the selected cell
glm::vec3 cellPosition = glm::vec3(selectedCell.cellData.positionAndMass);
selectedCell.dragDistance = glm::distance(rayOrigin, cellPosition);

// Calculate drag offset for smooth dragging
glm::vec3 mouseWorldPos = rayOrigin + rayDirection * selectedCell.dragDistance;
selectedCell.dragOffset = cellPosition - mouseWorldPos;

isDraggingCell = true;

    // Debug output with lineage information
    std::string lineageStr = getCellLineageString(selectedIndex);
    std::cout << "Selected cell " << selectedIndex << " (lineage: " << lineageStr << ") at distance " << selectedCell.dragDistance << "\n";
}
else
{
clearSelection();
}
}

if (isDraggingCell && isMouseDown && selectedCell.isValid)
{
// Continue dragging at the maintained distance
glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
glm::vec3 newWorldPos = camera.getPosition() + rayDirection * selectedCell.dragDistance;
dragSelectedCell(newWorldPos + selectedCell.dragOffset);
}
if (!isMouseDown)
{
if (isDraggingCell)
{
endDrag();
}
}
}

// todo: REWRITE FOR GPU ONLY
int CellManager::selectCellAtPosition(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection)
{
float closestDistance = FLT_MAX;
int closestCellIndex = -1;
int intersectionCount = 0;

// Debug output for raycasting
std::cout << "Testing " << totalCellCount << " cells for intersection..." << std::endl;

for (int i = 0; i < totalCellCount; i++)
{
glm::vec3 cellPosition = glm::vec3(cpuCells[i].positionAndMass);
float cellRadius = cpuCells[i].getRadius();

float intersectionDistance;
    if (raySphereIntersection(rayOrigin, rayDirection, cellPosition, cellRadius, intersectionDistance))
    {
        intersectionCount++;
        std::string lineageStr = getCellLineageString(i);
        std::cout << "Cell " << i << " (lineage: " << lineageStr << ") at (" << cellPosition.x << ", " << cellPosition.y << ", " << cellPosition.z
        << ") radius " << cellRadius << " intersected at distance " << intersectionDistance << std::endl;

if (intersectionDistance < closestDistance && intersectionDistance > 0)
{
closestDistance = intersectionDistance;
closestCellIndex = i;
}
}
}

std::cout << "Found " << intersectionCount << " intersections total" << std::endl;
if (closestCellIndex >= 0)
{
std::cout << "Selected closest cell " << closestCellIndex << " at distance " << closestDistance << std::endl;
}
else
{
std::cout << "No valid cell intersections found" << std::endl;
}

return closestCellIndex;
}

// todo: REWRITE FOR GPU ONLY
void CellManager::dragSelectedCell(const glm::vec3 &newWorldPosition)
{
if (!selectedCell.isValid)
return;

// Update CPU cell data
cpuCells[selectedCell.cellIndex].positionAndMass.x = newWorldPosition.x;
cpuCells[selectedCell.cellIndex].positionAndMass.y = newWorldPosition.y;
cpuCells[selectedCell.cellIndex].positionAndMass.z = newWorldPosition.z;

// Clear velocity when dragging to prevent conflicts with physics
cpuCells[selectedCell.cellIndex].velocity.x = 0.0f;
cpuCells[selectedCell.cellIndex].velocity.y = 0.0f;
cpuCells[selectedCell.cellIndex].velocity.z = 0.0f; // Update cached selected cell data
selectedCell.cellData = cpuCells[selectedCell.cellIndex];

// Update GPU buffers immediately to ensure compute shaders see the new position
for (int i = 0; i < 3; i++)
{
glNamedBufferSubData(cellBuffer[i],
selectedCell.cellIndex * sizeof(ComputeCell),
sizeof(ComputeCell),
&cpuCells[selectedCell.cellIndex]);
}
}

void CellManager::clearSelection()
{
    selectedCell.isValid = false;
    selectedCell.cellIndex = -1;
    isDraggingCell = false;
}

void CellManager::refreshSelectedCellData()
{
    if (!selectedCell.isValid || selectedCell.cellIndex < 0 || selectedCell.cellIndex >= totalCellCount)
    {
        return;
    }
    
    // Read the latest cell data from GPU buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, getCellReadBuffer());
    ComputeCell* cellData = (ComputeCell*)glMapBufferRange(
        GL_SHADER_STORAGE_BUFFER,
        selectedCell.cellIndex * sizeof(ComputeCell),
        sizeof(ComputeCell),
        GL_MAP_READ_BIT
    );
    
    if (cellData)
    {
        selectedCell.cellData = *cellData;
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::endDrag()
{
if (isDraggingCell && selectedCell.isValid)
{
// Reset velocity to zero when ending drag to prevent sudden jumps
cpuCells[selectedCell.cellIndex].velocity.x = 0.0f;
cpuCells[selectedCell.cellIndex].velocity.y = 0.0f;
cpuCells[selectedCell.cellIndex].velocity.z = 0.0f; // Update the GPU buffers with the final state
for (int i = 0; i < 3; i++)
{
glNamedBufferSubData(cellBuffer[i],
  selectedCell.cellIndex * sizeof(ComputeCell),
  sizeof(ComputeCell),
  &cpuCells[selectedCell.cellIndex]);
}
}

isDraggingCell = false;
}

void CellManager::syncCellPositionsFromGPU()
{ // This will fail if the CPU buffer has the wrong size, which will happen once cell division is implemented so i will have to rewrite this
if (totalCellCount == 0)
return;

// OPTIMIZED: Use barrier batching for GPU sync
addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
flushBarriers();

// Copy data from GPU buffer to staging buffer (no GPU->CPU transfer warning)
glCopyNamedBufferSubData(getCellReadBuffer(), stagingCellBuffer, 0, 0, totalCellCount * sizeof(ComputeCell));

// Memory barrier to ensure copy is complete
addBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
flushBarriers();

// CRITICAL FIX: Use fence sync with longer timeout to ensure GPU operations are complete
// This prevents the pixel transfer synchronization warning
GLsync sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
GLenum result = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, 10000000); // Wait up to 10ms
glDeleteSync(sync);

// If the sync didn't complete in time, fall back to glFinish
if (result == GL_TIMEOUT_EXPIRED) {
glFinish();
}

// Now read from the staging buffer (CPU->CPU, no warning)
ComputeCell* stagedData = static_cast<ComputeCell*>(mappedCellPtr);

if (stagedData)
{
cpuCells.reserve(totalCellCount);
// Copy data from staging buffer to CPU storage
for (int i = 0; i < totalCellCount; i++)
{
if (i < cpuCells.size())
{
cpuCells[i] = stagedData[i]; // Sync all data
} else
{
cpuCells.push_back(stagedData[i]);
}
}


}
else
{
std::cerr << "Failed to access staging buffer for cell data readback" << std::endl;
}
}

// todo: REWRITE FOR GPU ONLY
glm::vec3 CellManager::calculateMouseRay(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
          const Camera &camera)
{
// Safety check for zero screen size
if (screenSize.x <= 0 || screenSize.y <= 0)
{
return camera.getFront(); // Return camera forward direction as fallback
}

// Convert screen coordinates to normalized device coordinates (-1 to 1)
// Screen coordinates: (0,0) is top-left, (width,height) is bottom-right
// NDC coordinates: (-1,-1) is bottom-left, (1,1) is top-right
float x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
float y = 1.0f - (2.0f * mousePos.y) / screenSize.y; // Convert from screen Y (top-down) to NDC Y (bottom-up)

// Create projection matrix (matching the one used in rendering)
float aspectRatio = screenSize.x / screenSize.y;
if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
{
return camera.getFront(); // Return camera forward direction as fallback
}

glm::mat4 projection = glm::perspective(glm::radians(45.0f),
             aspectRatio,
             0.1f, 1000.0f);

// Create view matrix
glm::mat4 view = camera.getViewMatrix();

// Calculate inverse view-projection matrix with error checking
glm::mat4 viewProjection = projection * view;
glm::mat4 inverseVP;

// Check if the matrix is invertible
float determinant = glm::determinant(viewProjection);
if (abs(determinant) < 1e-6f)
{
return camera.getFront(); // Return camera forward direction as fallback
}

inverseVP = glm::inverse(viewProjection);

// Create normalized device coordinate points for near and far planes
glm::vec4 rayClipNear = glm::vec4(x, y, -1.0f, 1.0f);
glm::vec4 rayClipFar = glm::vec4(x, y, 1.0f, 1.0f);

// Transform to world space
glm::vec4 rayWorldNear = inverseVP * rayClipNear;
glm::vec4 rayWorldFar = inverseVP * rayClipFar;

// Convert from homogeneous coordinates with safety checks
if (abs(rayWorldNear.w) < 1e-6f || abs(rayWorldFar.w) < 1e-6f)
{
return camera.getFront(); // Return camera forward direction as fallback
}

rayWorldNear /= rayWorldNear.w;
rayWorldFar /= rayWorldFar.w;

// Calculate ray direction
glm::vec3 rayDirection = glm::vec3(rayWorldFar) - glm::vec3(rayWorldNear);

// Check if the direction is valid and normalize
if (glm::length(rayDirection) < 1e-6f)
{
return camera.getFront(); // Return camera forward direction as fallback
}

rayDirection = glm::normalize(rayDirection);

// Final validation
if (!std::isfinite(rayDirection.x) || !std::isfinite(rayDirection.y) || !std::isfinite(rayDirection.z))
{
return camera.getFront(); // Return camera forward direction as fallback
}

return rayDirection;
}

// todo: REWRITE FOR GPU ONLY
bool CellManager::raySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
         const glm::vec3 &sphereCenter, float sphereRadius, float &distance)
{
glm::vec3 oc = rayOrigin - sphereCenter;
float a = glm::dot(rayDirection, rayDirection);
float b = 2.0f * glm::dot(oc, rayDirection);
float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;

float discriminant = b * b - 4 * a * c;

if (discriminant < 0)
{
return false; // No intersection
}

float sqrtDiscriminant = sqrt(discriminant);

// Calculate both possible intersection points
float t1 = (-b - sqrtDiscriminant) / (2.0f * a);
float t2 = (-b + sqrtDiscriminant) / (2.0f * a);

// Use the closest positive intersection (in front of the ray origin)
if (t1 > 0.001f)
{ // Small epsilon to avoid self-intersection
distance = t1;
return true;
}
else if (t2 > 0.001f)
{
distance = t2;
return true;
}

return false; // Both intersections are behind the ray origin or too close
}
