#include "cell_manager.h"
#include "camera.h"
#include "config.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <GLFW/glfw3.h>
#include <gtc/matrix_transform.hpp>

CellManager::CellManager() {
    // Generate sphere mesh
    sphereMesh.generateSphere(12, 16, 1.0f); // Even lower poly count: 12x16 = 192 triangles
    sphereMesh.setupBuffers();

    initializeGPUBuffers();
    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell_physics.comp");
    updateShader = new Shader("shaders/cell_update.comp");
    extractShader = new Shader("shaders/extract_instances.comp");
}

CellManager::~CellManager() {
    cleanup();
}

void CellManager::initializeGPUBuffers() {
    // Create compute buffer for cell data
    glGenBuffers(1, &cellBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * sizeof(ComputeCell), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Create instance buffer for rendering (contains position + radius)
    glGenBuffers(1, &instanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER, config::MAX_CELLS * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Setup the sphere mesh to use our instance buffer
    sphereMesh.setupInstanceBuffer(instanceBuffer);

    // Reserve CPU storage
    cpuCells.reserve(config::MAX_CELLS);
}

void CellManager::renderCells(glm::vec2 resolution, Shader& cellShader, Camera& camera) {
    if (cell_count == 0) return;
    
    // Safety check for zero-sized framebuffer (minimized window)
    if (resolution.x <= 0 || resolution.y <= 0) {
        return;
    }
    
    // Additional safety check for very small resolutions that might cause issues
    if (resolution.x < 1 || resolution.y < 1) {
        return;
    }

    try {

    // Use compute shader to efficiently extract instance data
    extractShader->use();
    extractShader->setInt("u_cellCount", cell_count);

    // Bind buffers for compute shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, instanceBuffer);

    // Dispatch extract compute shader
    GLuint numGroups = (cell_count + 63) / 64; // 64 threads per group
    extractShader->dispatch(numGroups, 1, 1);

    // Memory barrier to ensure data is ready for rendering
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Use the sphere shader
    cellShader.use();    // Set up camera matrices (only calculate once per frame, not per cell)
    glm::mat4 view = camera.getViewMatrix();
    
    // Calculate aspect ratio with safety check
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
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
    
    // Set selection highlighting uniforms
    if (selectedCell.isValid) {
        glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndRadius);
        float selectedRadius = selectedCell.cellData.positionAndRadius.w;
        cellShader.setVec3("uSelectedCellPos", selectedPos);
        cellShader.setFloat("uSelectedCellRadius", selectedRadius);
    } else {
        cellShader.setVec3("uSelectedCellPos", glm::vec3(-9999.0f)); // Invalid position
        cellShader.setFloat("uSelectedCellRadius", 0.0f);
    }
    cellShader.setFloat("uTime", static_cast<float>(glfwGetTime()));    // Enable depth testing for proper 3D rendering (don't clear here - already done in main loop)
    glEnable(GL_DEPTH_TEST);

    // Render instanced spheres
    sphereMesh.render(cell_count);
    
    } catch (const std::exception& e) {
        std::cerr << "Exception in renderCells: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception in renderCells" << std::endl;
    }
}

void CellManager::addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius) {
    if (cell_count >= config::MAX_CELLS) {
        std::cout << "Warning: Maximum cell count reached!" << std::endl;
        return;
    }

    // Create new compute cell
    ComputeCell newCell;
    newCell.positionAndRadius = glm::vec4(position, radius);
    newCell.velocityAndMass = glm::vec4(velocity, mass);
    newCell.acceleration = glm::vec4(0.0f);

    // Add to CPU storage
    cpuCells.push_back(newCell);
    cell_count++;

    // Update GPU buffer
    updateGPUBuffers();
}

void CellManager::updateCells(float deltaTime) {
    if (cell_count == 0) return; 

    // Run physics computation on GPU
    runPhysicsCompute(deltaTime);

    // Add memory barrier to ensure physics calculations are complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Run position/velocity update on GPU
    runUpdateCompute(deltaTime);

    // Add memory barrier to ensure all computations are complete
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::cleanup() {
    if (cellBuffer != 0) {
        glDeleteBuffers(1, &cellBuffer);
        cellBuffer = 0;
    }
    if (instanceBuffer != 0) {
        glDeleteBuffers(1, &instanceBuffer);
        instanceBuffer = 0;
    }    if (extractShader) {
        extractShader->destroy();
        delete extractShader;
        extractShader = nullptr;
    }
    if (physicsShader) {
        physicsShader->destroy();
        delete physicsShader;
        physicsShader = nullptr;
    }
    if (updateShader) {
        updateShader->destroy();
        delete updateShader;
        updateShader = nullptr;
    }

    sphereMesh.cleanup();
}

void CellManager::updateGPUBuffers() {
    // Upload CPU cell data to GPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, cell_count * sizeof(ComputeCell), cpuCells.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runPhysicsCompute(float deltaTime) {
    physicsShader->use();

    // Set uniforms
    physicsShader->setFloat("u_deltaTime", deltaTime);
    physicsShader->setInt("u_cellCount", cell_count);
    physicsShader->setFloat("u_damping", 0.98f);
    
    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    physicsShader->setInt("u_draggedCellIndex", draggedIndex);

    // Bind cell buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);

    // Dispatch compute shader
    // Use work groups of 64 threads each
    GLuint numGroups = (cell_count + 63) / 64; // Round up division
    physicsShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runUpdateCompute(float deltaTime) {
    updateShader->use();

    // Set uniforms
    updateShader->setFloat("u_deltaTime", deltaTime);
    updateShader->setInt("u_cellCount", cell_count);
    updateShader->setFloat("u_damping", 0.98f);
    
    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    updateShader->setInt("u_draggedCellIndex", draggedIndex);

    // Bind cell buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cell_count + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::spawnCells(int count) {
    for (int i = 0; i < count && cell_count < config::MAX_CELLS; ++i) {
        // Random position within spawn radius
        float angle1 = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float angle2 = static_cast<float>(rand()) / RAND_MAX * 3.14159f;
        float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;

        glm::vec3 position = glm::vec3(
            radius * sin(angle2) * cos(angle1),
            radius * cos(angle2),
            radius * sin(angle2) * sin(angle1)
        );

        // Random velocity
        glm::vec3 velocity = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f
        );

        // Random mass and radius
        float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        float cellRadius = 0.5f + static_cast<float>(rand()) / RAND_MAX * 1.0f;

        addCell(position, velocity, mass, cellRadius);
    }
}

// Cell selection and interaction implementation
void CellManager::handleMouseInput(const glm::vec2& mousePos, const glm::vec2& screenSize, 
                                  const Camera& camera, bool isMousePressed, bool isMouseDown, 
                                  float scrollDelta) {
    // Safety check for invalid screen size (minimized window)
    if (screenSize.x <= 0 || screenSize.y <= 0) {
        return;
    }
    
    // Handle scroll wheel to adjust drag distance when cell is selected
    if (selectedCell.isValid && scrollDelta != 0.0f) {
        float scrollSensitivity = 2.0f;
        selectedCell.dragDistance += scrollDelta * scrollSensitivity;
        selectedCell.dragDistance = glm::clamp(selectedCell.dragDistance, 1.0f, 100.0f);
        
        // If we're dragging, update the cell position to maintain the new distance
        if (isDraggingCell) {
            glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
            glm::vec3 newWorldPos = camera.getPosition() + rayDirection * selectedCell.dragDistance;
            dragSelectedCell(newWorldPos);
        }
    }    if (isMousePressed && !isDraggingCell) {
        // Sync current cell positions from GPU before attempting selection
        syncCellPositionsFromGPU();
        
        // Start new selection with improved raycasting
        glm::vec3 rayOrigin = camera.getPosition();
        glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
          // Debug: Print mouse coordinates and ray info (reduced logging)
        std::cout << "Mouse click at (" << mousePos.x << ", " << mousePos.y << ")" << std::endl;
        
        int selectedIndex = selectCellAtPosition(rayOrigin, rayDirection);
        if (selectedIndex >= 0) {
            selectedCell.cellIndex = selectedIndex;
            selectedCell.cellData = cpuCells[selectedIndex];
            selectedCell.isValid = true;
            
            // Calculate the distance from camera to the selected cell
            glm::vec3 cellPosition = glm::vec3(selectedCell.cellData.positionAndRadius);
            selectedCell.dragDistance = glm::distance(rayOrigin, cellPosition);
            
            // Calculate drag offset for smooth dragging
            glm::vec3 mouseWorldPos = rayOrigin + rayDirection * selectedCell.dragDistance;
            selectedCell.dragOffset = cellPosition - mouseWorldPos;
            
            isDraggingCell = true;
            
            // Debug output
            std::cout << "Selected cell " << selectedIndex << " at distance " << selectedCell.dragDistance << std::endl;
        } else {
            clearSelection();
        }
    }
    
    if (isDraggingCell && isMouseDown && selectedCell.isValid) {
        // Continue dragging at the maintained distance
        glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);
        glm::vec3 newWorldPos = camera.getPosition() + rayDirection * selectedCell.dragDistance;
        dragSelectedCell(newWorldPos + selectedCell.dragOffset);
    }
      if (!isMouseDown) {
        if (isDraggingCell) {
            endDrag();
        }
    }
}

int CellManager::selectCellAtPosition(const glm::vec3& rayOrigin, const glm::vec3& rayDirection) {
    float closestDistance = FLT_MAX;
    int closestCellIndex = -1;
    int intersectionCount = 0;
    
    // Debug output for raycasting
    std::cout << "Testing " << cell_count << " cells for intersection..." << std::endl;
    
    for (int i = 0; i < cell_count; i++) {
        glm::vec3 cellPosition = glm::vec3(cpuCells[i].positionAndRadius);
        float cellRadius = cpuCells[i].positionAndRadius.w;
        
        float intersectionDistance;
        if (raySphereIntersection(rayOrigin, rayDirection, cellPosition, cellRadius, intersectionDistance)) {
            intersectionCount++;
            std::cout << "Cell " << i << " at (" << cellPosition.x << ", " << cellPosition.y << ", " << cellPosition.z 
                     << ") radius " << cellRadius << " intersected at distance " << intersectionDistance << std::endl;
                     
            if (intersectionDistance < closestDistance && intersectionDistance > 0) {
                closestDistance = intersectionDistance;
                closestCellIndex = i;
            }
        }
    }
    
    std::cout << "Found " << intersectionCount << " intersections total" << std::endl;
    if (closestCellIndex >= 0) {
        std::cout << "Selected closest cell " << closestCellIndex << " at distance " << closestDistance << std::endl;
    } else {
        std::cout << "No valid cell intersections found" << std::endl;
    }
    
    return closestCellIndex;
}

void CellManager::dragSelectedCell(const glm::vec3& newWorldPosition) {
    if (!selectedCell.isValid) return;
    
    // Update CPU cell data
    cpuCells[selectedCell.cellIndex].positionAndRadius.x = newWorldPosition.x;
    cpuCells[selectedCell.cellIndex].positionAndRadius.y = newWorldPosition.y;
    cpuCells[selectedCell.cellIndex].positionAndRadius.z = newWorldPosition.z;
    
    // Clear velocity when dragging to prevent conflicts with physics
    cpuCells[selectedCell.cellIndex].velocityAndMass.x = 0.0f;
    cpuCells[selectedCell.cellIndex].velocityAndMass.y = 0.0f;
    cpuCells[selectedCell.cellIndex].velocityAndMass.z = 0.0f;
    
    // Update cached selected cell data
    selectedCell.cellData = cpuCells[selectedCell.cellIndex];
    
    // Update GPU buffer immediately to ensure compute shaders see the new position
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 
                   selectedCell.cellIndex * sizeof(ComputeCell), 
                   sizeof(ComputeCell), 
                   &cpuCells[selectedCell.cellIndex]);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::clearSelection() {
    selectedCell.cellIndex = -1;
    selectedCell.isValid = false;
    isDraggingCell = false;
}

void CellManager::endDrag() {
    if (isDraggingCell && selectedCell.isValid) {
        // Reset velocity to zero when ending drag to prevent sudden jumps
        cpuCells[selectedCell.cellIndex].velocityAndMass.x = 0.0f;
        cpuCells[selectedCell.cellIndex].velocityAndMass.y = 0.0f;
        cpuCells[selectedCell.cellIndex].velocityAndMass.z = 0.0f;
        
        // Update the GPU buffer with the final state
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 
                       selectedCell.cellIndex * sizeof(ComputeCell), 
                       sizeof(ComputeCell), 
                       &cpuCells[selectedCell.cellIndex]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    
    isDraggingCell = false;
}

void CellManager::syncCellPositionsFromGPU() {
    if (cell_count == 0) return;
    
    // Bind the cell buffer and read current positions back to CPU
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    
    // Use glMapBuffer for efficient GPU->CPU data transfer
    ComputeCell* gpuData = static_cast<ComputeCell*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY));
    
    if (gpuData) {
        // Copy only the position data from GPU to CPU (don't overwrite velocity/mass as those might be needed)
        for (int i = 0; i < cell_count; i++) {
            // Only sync position and radius, preserve velocity and mass from CPU
            cpuCells[i].positionAndRadius = gpuData[i].positionAndRadius;
        }
        
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        
        std::cout << "Synced " << cell_count << " cell positions from GPU" << std::endl;
    } else {
        std::cerr << "Failed to map GPU buffer for readback" << std::endl;
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

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
    
    float discriminant = b * b - 4 * a * c;
    
    if (discriminant < 0) {
        return false; // No intersection
    }
    
    float sqrtDiscriminant = sqrt(discriminant);
    
    // Calculate both possible intersection points
    float t1 = (-b - sqrtDiscriminant) / (2.0f * a);
    float t2 = (-b + sqrtDiscriminant) / (2.0f * a);
    
    // Use the closest positive intersection (in front of the ray origin)
    if (t1 > 0.001f) { // Small epsilon to avoid self-intersection
        distance = t1;
        return true;
    } else if (t2 > 0.001f) {
        distance = t2;
        return true;
    }
    
    return false; // Both intersections are behind the ray origin or too close
}

ComputeCell CellManager::getCellData(int index) const {
    if (index >= 0 && index < cell_count) {
        return cpuCells[index];
    }
    return ComputeCell{}; // Return empty cell if index is invalid
}

void CellManager::updateCellData(int index, const ComputeCell& newData) {
    if (index >= 0 && index < cell_count) {
        cpuCells[index] = newData;
        
        // Update selected cell cache if this is the selected cell
        if (selectedCell.isValid && selectedCell.cellIndex == index) {
            selectedCell.cellData = newData;
        }
        
        // Update only the specific cell in the GPU buffer (like dragSelectedCell does)
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 
                       index * sizeof(ComputeCell), 
                       sizeof(ComputeCell), 
                       &cpuCells[index]);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
}