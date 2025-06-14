#include "cell_manager.h"
#include "camera.h"
#include "config.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "timer.h"

CellManager::CellManager()
{
    // Generate sphere mesh
    sphereMesh.generateSphere(12, 16, 1.0f); // Even lower poly count: 12x16 = 192 triangles
    sphereMesh.setupBuffers();
    initializeGPUBuffers();
    // Initialize compute shaders for SoA layout
    physicsShader = new Shader("shaders/cell_physics_soa.comp");
    updateShader = new Shader("shaders/cell_update_soa.comp");
    extractShader = new Shader("shaders/extract_instances_soa.comp");
}

CellManager::~CellManager()
{
    cleanup();
}

void CellManager::initializeGPUBuffers()
{
    // Create separate SSBO buffers for each array in SoA layout
    // This provides much better memory access patterns for GPU compute shaders

    // Position buffer (vec3 positions)
    glGenBuffers(1, &positionBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Radius buffer (float radii)
    glGenBuffers(1, &radiusBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, radiusBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Velocity buffer (vec3 velocities)
    glGenBuffers(1, &velocityBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Mass buffer (float masses)
    glGenBuffers(1, &massBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, massBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Acceleration buffer (vec3 accelerations)
    glGenBuffers(1, &accelerationBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, accelerationBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * sizeof(glm::vec3), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Instance buffer for rendering (contains position + radius)
    glGenBuffers(1, &instanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER, config::MAX_CELLS * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Setup the sphere mesh to use our instance buffer
    sphereMesh.setupInstanceBuffer(instanceBuffer);

    // Reserve CPU SoA storage
    cellData.reserve(config::MAX_CELLS);
}

void CellManager::renderCells(glm::vec2 resolution, Shader &cellShader, Camera &camera)
{
    if (cellCount == 0)
        return;

    // Safety check for zero-sized framebuffer (minimized window)
    if (resolution.x <= 0 || resolution.y <= 0)
    {
        return;
    }

    // Additional safety check for very small resolutions that might cause issues
    if (resolution.x < 1 || resolution.y < 1)
    {
        return;
    }

    try
    { // Use compute shader to efficiently extract instance data from SoA buffers
        extractShader->use();
        extractShader->setInt("u_cellCount", cellCount);

        // Bind SoA buffers for compute shader
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer); // positions
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, radiusBuffer);   // radii
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceBuffer); // output instance data

        // Dispatch extract compute shader
        GLuint numGroups = (cellCount + 63) / 64; // 64 threads per group
        extractShader->dispatch(numGroups, 1, 1);

        // Memory barrier to ensure data is ready for rendering
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

        // Use the sphere shader
        cellShader.use(); // Set up camera matrices (only calculate once per frame, not per cell)
        glm::mat4 view = camera.getViewMatrix();

        // Calculate aspect ratio with safety check
        float aspectRatio = resolution.x / resolution.y;
        if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
        {
            // Use a default aspect ratio if calculation fails
            aspectRatio = 16.0f / 9.0f;
        }

        glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                                aspectRatio,
                                                0.1f, 1000.0f);
        // Set uniforms
        cellShader.setMat4("uProjection", projection);
        cellShader.setMat4("uView", view);
        cellShader.setVec3("uCameraPos", camera.getPosition());
        cellShader.setVec3("uLightDir", glm::vec3(1.0f, 1.0f, 1.0f));
        if (selectedCell.isValid)
        {
            glm::vec3 selectedPos = cellData.positions[selectedCell.cellIndex];
            float selectedRadius = cellData.radii[selectedCell.cellIndex];
            cellShader.setVec3("uSelectedCellPos", selectedPos);
            cellShader.setFloat("uSelectedCellRadius", selectedRadius);
        }
        else
        {
            cellShader.setVec3("uSelectedCellPos", glm::vec3(-9999.0f)); // Invalid position
            cellShader.setFloat("uSelectedCellRadius", 0.0f);
        }
        cellShader.setFloat("uTime", static_cast<float>(glfwGetTime())); // Enable depth testing for proper 3D rendering (don't clear here - already done in main loop)
        glEnable(GL_DEPTH_TEST);

        // Render instanced spheres
        sphereMesh.render(cellCount);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Exception in renderCells: " << e.what() << "\n";
    }
    catch (...)
    {
        std::cerr << "Unknown exception in renderCells\n";
    }
}

// SoA conversion and management functions

void CellManager::addCellToStagingBuffer(const ComputeCell &newCell)
{
    if (cellCount + cellStagingBuffer.size() + 1 > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    // Add to staging buffer for later batch processing
    cellStagingBuffer.push_back(newCell);
    pendingCellCount++;
}

void CellManager::addStagedCellsToGPU()
{
    if (cellStagingBuffer.empty())
        return;

    // Convert AoS staging buffer to SoA
    CellDataSoA stagingSoA;
    convertAoSToSoA(cellStagingBuffer, stagingSoA);

    // Add to GPU
    addCellsToGPU(stagingSoA, cellStagingBuffer.size());

    // Clear staging buffer
    cellStagingBuffer.clear();
    pendingCellCount = 0;
}

void CellManager::updateCellData(int index, const ComputeCell &newData)
{
    if (index >= 0 && index < cellCount)
    {
        // Update SoA data
        cellData.positions[index] = glm::vec3(newData.positionAndRadius);
        cellData.radii[index] = newData.positionAndRadius.w;
        cellData.velocities[index] = glm::vec3(newData.velocityAndMass);
        cellData.masses[index] = newData.velocityAndMass.w;
        cellData.accelerations[index] = glm::vec3(newData.acceleration);

        // Update selected cell cache if this is the selected cell
        if (selectedCell.isValid && selectedCell.cellIndex == index)
        {
            selectedCell.cellData = newData;
        }

        // Update GPU buffers
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(glm::vec3), sizeof(glm::vec3), &cellData.positions[index]);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, radiusBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(float), sizeof(float), &cellData.radii[index]);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(glm::vec3), sizeof(glm::vec3), &cellData.velocities[index]);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, massBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(float), sizeof(float), &cellData.masses[index]);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, accelerationBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, index * sizeof(glm::vec3), sizeof(glm::vec3), &cellData.accelerations[index]);

        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}

void CellManager::updateCells(float deltaTime)
{
    if (pendingCellCount > 0)
    {
        addStagedCellsToGPU(); // Sync any pending cells to GPU
    }

    TimerGPU timer("Cell Physics Update");

    if (cellCount == 0)
        return;

    // Run physics computation on GPU
    runPhysicsCompute(deltaTime);

    // Add memory barrier to ensure physics calculations are complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Run position/velocity update on GPU
    runUpdateCompute(deltaTime);

    // Add memory barrier to ensure all computations are complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::cleanup()
{
    // Clean up SoA buffers
    if (positionBuffer != 0)
    {
        glDeleteBuffers(1, &positionBuffer);
        positionBuffer = 0;
    }
    if (radiusBuffer != 0)
    {
        glDeleteBuffers(1, &radiusBuffer);
        radiusBuffer = 0;
    }
    if (velocityBuffer != 0)
    {
        glDeleteBuffers(1, &velocityBuffer);
        velocityBuffer = 0;
    }
    if (massBuffer != 0)
    {
        glDeleteBuffers(1, &massBuffer);
        massBuffer = 0;
    }
    if (accelerationBuffer != 0)
    {
        glDeleteBuffers(1, &accelerationBuffer);
        accelerationBuffer = 0;
    }
    if (instanceBuffer != 0)
    {
        glDeleteBuffers(1, &instanceBuffer);
        instanceBuffer = 0;
    }

    // Clean up shaders
    if (extractShader)
    {
        extractShader->destroy();
        delete extractShader;
        extractShader = nullptr;
    }
    if (physicsShader)
    {
        physicsShader->destroy();
        delete physicsShader;
        physicsShader = nullptr;
    }
    if (updateShader)
    {
        updateShader->destroy();
        delete updateShader;
        updateShader = nullptr;
    }

    sphereMesh.cleanup();
}

void CellManager::runPhysicsCompute(float deltaTime)
{
    physicsShader->use();

    // Set uniforms
    physicsShader->setFloat("u_deltaTime", deltaTime);
    physicsShader->setInt("u_cellCount", cellCount);
    physicsShader->setFloat("u_damping", 0.98f);

    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    physicsShader->setInt("u_draggedCellIndex", draggedIndex);

    // Bind SoA buffers for compute shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, radiusBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, velocityBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, massBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, accelerationBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    physicsShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runUpdateCompute(float deltaTime)
{
    updateShader->use();

    // Set uniforms
    updateShader->setFloat("u_deltaTime", deltaTime);
    updateShader->setInt("u_cellCount", cellCount);
    updateShader->setFloat("u_damping", 0.98f);

    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    updateShader->setInt("u_draggedCellIndex", draggedIndex);

    // Bind SoA buffers for compute shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, positionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, radiusBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, velocityBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, massBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, accelerationBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::spawnCells(int count)
{
    TimerCPU cpuTimer("Spawning Cells", true);

    // Create temporary SoA data for the new cells
    CellDataSoA newCells;
    newCells.resize(count);

    for (int i = 0; i < count && cellCount + i < config::MAX_CELLS; ++i)
    {
        // Random position within spawn radius
        float angle1 = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float angle2 = static_cast<float>(rand()) / RAND_MAX * 3.14159f;
        float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;

        newCells.positions[i] = glm::vec3(
            radius * sin(angle2) * cos(angle1),
            radius * cos(angle2),
            radius * sin(angle2) * sin(angle1));

        // Random velocity
        newCells.velocities[i] = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f);

        // Random mass and radius
        newCells.masses[i] = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        newCells.radii[i] = 0.5f + static_cast<float>(rand()) / RAND_MAX * 1.0f;
        newCells.accelerations[i] = glm::vec3(0.0f); // Reset acceleration
    }

    // Add directly to GPU using SoA
    addCellsToGPU(newCells, count);
}
// Cell selection and interaction implementation // todo: REWRITE FOR GPU ONLY
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
            selectedCell.cellData = getAoSCell(selectedIndex); // Convert from SoA to legacy format
            selectedCell.isValid = true;

            // Calculate the distance from camera to the selected cell
            glm::vec3 cellPosition = cellData.positions[selectedIndex];
            selectedCell.dragDistance = glm::distance(rayOrigin, cellPosition);

            // Calculate drag offset for smooth dragging
            glm::vec3 mouseWorldPos = rayOrigin + rayDirection * selectedCell.dragDistance;
            selectedCell.dragOffset = cellPosition - mouseWorldPos;

            isDraggingCell = true;

            // Debug output
            std::cout << "Selected cell " << selectedIndex << " at distance " << selectedCell.dragDistance << "\n";
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
    std::cout << "Testing " << cellCount << " cells for intersection..." << std::endl;
    for (int i = 0; i < cellCount; i++)
    {
        glm::vec3 cellPosition = cellData.positions[i];
        float cellRadius = cellData.radii[i];

        float intersectionDistance;
        if (raySphereIntersection(rayOrigin, rayDirection, cellPosition, cellRadius, intersectionDistance))
        {
            intersectionCount++;
            std::cout << "Cell " << i << " at (" << cellPosition.x << ", " << cellPosition.y << ", " << cellPosition.z
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

    // Update SoA data
    cellData.positions[selectedCell.cellIndex] = newWorldPosition;

    // Clear velocity when dragging to prevent conflicts with physics
    cellData.velocities[selectedCell.cellIndex] = glm::vec3(0.0f);

    // Update cached selected cell data (convert to legacy format)
    selectedCell.cellData = getAoSCell(selectedCell.cellIndex);

    // Update GPU buffers immediately to ensure compute shaders see the new position
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    selectedCell.cellIndex * sizeof(glm::vec3),
                    sizeof(glm::vec3),
                    &cellData.positions[selectedCell.cellIndex]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    selectedCell.cellIndex * sizeof(glm::vec3),
                    sizeof(glm::vec3),
                    &cellData.velocities[selectedCell.cellIndex]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::clearSelection()
{
    selectedCell.cellIndex = -1;
    selectedCell.isValid = false;
    isDraggingCell = false;
}

void CellManager::endDrag()
{
    if (isDraggingCell && selectedCell.isValid)
    {
        // Reset velocity to zero when ending drag to prevent sudden jumps
        cellData.velocities[selectedCell.cellIndex] = glm::vec3(0.0f);

        // Update the GPU buffer with the final state
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        selectedCell.cellIndex * sizeof(glm::vec3),
                        sizeof(glm::vec3),
                        &cellData.velocities[selectedCell.cellIndex]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }

    isDraggingCell = false;
}

void CellManager::syncCellPositionsFromGPU()
{
    if (cellCount == 0)
        return;

    // Read positions back from GPU for mouse interaction
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    glm::vec3 *gpuPositions = static_cast<glm::vec3 *>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));

    if (gpuPositions)
    {
        // Copy GPU positions to CPU SoA storage
        for (int i = 0; i < cellCount; i++)
        {
            cellData.positions[i] = gpuPositions[i];
        }

        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        std::cout << "Synced " << cellCount << " cell positions from GPU" << std::endl;
    }
    else
    {
        std::cerr << "Failed to map position buffer for reading!" << std::endl;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// Mouse interaction utility functions
glm::vec3 CellManager::calculateMouseRay(const glm::vec2& mousePos, const glm::vec2& screenSize, 
                                        const Camera& camera) {
    // Safety check for zero screen size
    if (screenSize.x <= 0 || screenSize.y <= 0) {
        return camera.getFront(); // Return camera forward direction as fallback
    }
    
    // Convert screen coordinates to normalized device coordinates (-1 to 1)
    // Screen coordinates: (0,0) is top-left, (width,height) is bottom-right
    // NDC coordinates: (-1,-1) is bottom-left, (1,1) is top-right
    float x = (2.0f * mousePos.x) / screenSize.x - 1.0f;
    float y = 1.0f - (2.0f * mousePos.y) / screenSize.y; // Convert from screen Y (top-down) to NDC Y (bottom-up)
    
    // Create projection matrix (matching the one used in rendering)
    float aspectRatio = screenSize.x / screenSize.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
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
    if (abs(determinant) < 1e-6f) {
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
    if (abs(rayWorldNear.w) < 1e-6f || abs(rayWorldFar.w) < 1e-6f) {
        return camera.getFront(); // Return camera forward direction as fallback
    }
    
    rayWorldNear /= rayWorldNear.w;
    rayWorldFar /= rayWorldFar.w;
    
    // Calculate ray direction
    glm::vec3 rayDirection = glm::vec3(rayWorldFar) - glm::vec3(rayWorldNear);
    
    // Check if the direction is valid and normalize
    if (glm::length(rayDirection) < 1e-6f) {
        return camera.getFront(); // Return camera forward direction as fallback
    }
    
    rayDirection = glm::normalize(rayDirection);
    
    // Final validation
    if (!std::isfinite(rayDirection.x) || !std::isfinite(rayDirection.y) || !std::isfinite(rayDirection.z)) {
        return camera.getFront(); // Return camera forward direction as fallback
    }
    
    return rayDirection;
}

bool CellManager::raySphereIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                                       const glm::vec3& sphereCenter, float sphereRadius, float& distance) {
    glm::vec3 oc = rayOrigin - sphereCenter;
    float a = glm::dot(rayDirection, rayDirection);
    float b = 2.0f * glm::dot(oc, rayDirection);
    float c = glm::dot(oc, oc) - sphereRadius * sphereRadius;
    
    float discriminant = b * b - 4.0f * a * c;
    
    if (discriminant < 0) {
        return false; // No intersection
    }
    
    float sqrt_discriminant = sqrt(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0f * a);
    float t2 = (-b + sqrt_discriminant) / (2.0f * a);
    
    // We want the closest positive intersection
    if (t1 > 0) {
        distance = t1;
        return true;
    } else if (t2 > 0) {
        distance = t2;
        return true;
    }
    
    return false; // Both intersections are behind the ray origin
}

// SoA conversion and management functions
void CellManager::convertAoSToSoA(const std::vector<ComputeCell> &aosData, CellDataSoA &soaData)
{
    int count = aosData.size();
    soaData.resize(count);

    for (int i = 0; i < count; i++)
    {
        soaData.positions[i] = glm::vec3(aosData[i].positionAndRadius);
        soaData.radii[i] = aosData[i].positionAndRadius.w;
        soaData.velocities[i] = glm::vec3(aosData[i].velocityAndMass);
        soaData.masses[i] = aosData[i].velocityAndMass.w;
        soaData.accelerations[i] = glm::vec3(aosData[i].acceleration);
    }
}

ComputeCell CellManager::getAoSCell(int index) const
{
    if (index < 0 || index >= cellCount)
    {
        return ComputeCell{}; // Return empty cell if index is invalid
    }

    ComputeCell cell;
    cell.positionAndRadius = glm::vec4(cellData.positions[index], cellData.radii[index]);
    cell.velocityAndMass = glm::vec4(cellData.velocities[index], cellData.masses[index]);
    cell.acceleration = glm::vec4(cellData.accelerations[index], 0.0f);
    return cell;
}

void CellManager::addCellsToGPU(const CellDataSoA &newCells, int count)
{
    if (cellCount + count > config::MAX_CELLS)
    {
        std::cout << "Warning: Cannot add " << count << " cells. Would exceed MAX_CELLS limit.\n";
        count = config::MAX_CELLS - cellCount;
    }

    if (count <= 0)
        return;

    // Update CPU storage
    int startIndex = cellCount;
    cellData.resize(cellCount + count);

    for (int i = 0; i < count; i++)
    {
        cellData.positions[startIndex + i] = newCells.positions[i];
        cellData.radii[startIndex + i] = newCells.radii[i];
        cellData.velocities[startIndex + i] = newCells.velocities[i];
        cellData.masses[startIndex + i] = newCells.masses[i];
        cellData.accelerations[startIndex + i] = newCells.accelerations[i];
    }

    // Upload to GPU buffers
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, startIndex * sizeof(glm::vec3),
                    count * sizeof(glm::vec3), &newCells.positions[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, radiusBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, startIndex * sizeof(float),
                    count * sizeof(float), &newCells.radii[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, startIndex * sizeof(glm::vec3),
                    count * sizeof(glm::vec3), &newCells.velocities[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, massBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, startIndex * sizeof(float),
                    count * sizeof(float), &newCells.masses[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, accelerationBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, startIndex * sizeof(glm::vec3),
                    count * sizeof(glm::vec3), &newCells.accelerations[0]);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    cellCount += count;
    std::cout << "Added " << count << " cells to GPU. Total: " << cellCount << "\n";
}
