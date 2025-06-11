#include "cell_manager.h"
#include "camera.h"
#include "config.h"
#include "genome_system.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <GLFW/glfw3.h>
#include <gtc/matrix_transform.hpp>

CellManager::CellManager() {
    // Initialize genome system first
    if (!g_genomeSystem) {
        g_genomeSystem = std::make_unique<GenomeSystem>();
    }
    
    // Generate sphere mesh
    sphereMesh.generateSphere(12, 16, 1.0f); // Even lower poly count: 12x16 = 192 triangles
    sphereMesh.setupBuffers();

    initializeGPUBuffers();
    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell_physics.comp");
    updateShader = new Shader("shaders/cell_update.comp");
    extractShader = new Shader("shaders/extract_instances.comp");
    
    // Register for genome changes
    g_genomeSystem->addChangeCallback([this]() {
        this->onGenomeChanged();
    });
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

    // Create genome mode buffer
    glGenBuffers(1, &genomeModeBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, genomeModeBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 64 * sizeof(GPUGenomeMode), nullptr, GL_DYNAMIC_DRAW); // Support up to 64 modes
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);    // Create instance buffer for rendering (contains position + radius + color)
    // This needs to be an SSBO since the compute shader writes to it
    glGenBuffers(1, &instanceBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, instanceBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, config::MAX_CELLS * 2 * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW); // Position+Radius and Color
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Setup the sphere mesh to use our instance buffer
    sphereMesh.setupInstanceBuffer(instanceBuffer);

    // Reserve CPU storage
    cpuCells.reserve(config::MAX_CELLS);
    
    // Initialize genome mode buffer with current data
    updateGenomeModeBuffer();
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

    try {    // Use compute shader to efficiently extract instance data
    extractShader->use();
    extractShader->setInt("u_cellCount", cell_count);
    
    // Pass genome mode count
    int genomeModeCount = g_genomeSystem ? static_cast<int>(g_genomeSystem->getModeCount()) : 1;
    extractShader->setInt("u_genomeModeCount", genomeModeCount);

    // Bind buffers for compute shader
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, genomeModeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceBuffer);

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

void CellManager::addCell(glm::vec3 position, glm::vec3 velocity, float mass, float radius, int modeIndex) {
    if (cell_count >= config::MAX_CELLS) {
        std::cout << "Warning: Maximum cell count reached!" << std::endl;
        return;
    }

    // Default to initial mode if not specified or invalid
    if (modeIndex < 0 && g_genomeSystem) {
        modeIndex = g_genomeSystem->getInitialModeIndex();
    }
    if (modeIndex < 0) modeIndex = 0; // Fallback

    // Create new compute cell
    ComputeCell newCell;
    newCell.positionAndRadius = glm::vec4(position, radius);
    newCell.velocityAndMass = glm::vec4(velocity, mass);
    newCell.acceleration = glm::vec4(0.0f);
    newCell.genomeData = glm::ivec4(modeIndex, 0, 0, 0); // modeIndex, splitTimer=0, adhesionCount=0, flags=0

    // Add to CPU storage
    cpuCells.push_back(newCell);
    cell_count++;

    // Update GPU buffer
    updateGPUBuffers();
}

void CellManager::addCellToStorage(glm::vec3 position, glm::vec3 velocity, float mass, float radius, int modeIndex) {
    if (cell_count >= config::MAX_CELLS) {
        std::cout << "Warning: Maximum cell count reached!" << std::endl;
        return;
    }

    // Default to initial mode if not specified or invalid
    if (modeIndex < 0 && g_genomeSystem) {
        modeIndex = g_genomeSystem->getInitialModeIndex();
    }
    if (modeIndex < 0) modeIndex = 0; // Fallback

    // Create new compute cell
    ComputeCell newCell;
    newCell.positionAndRadius = glm::vec4(position, radius);
    newCell.velocityAndMass = glm::vec4(velocity, mass);
    newCell.acceleration = glm::vec4(0.0f);
    newCell.genomeData = glm::ivec4(modeIndex, 0, 0, 0); // modeIndex, splitTimer=0, adhesionCount=0, flags=0

    // Add to CPU storage only (no immediate GPU sync)
    cpuCells.push_back(newCell);
    cell_count++;
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
    
    // Process any cell divisions that were flagged by the GPU
    processCellDivisions();
}

void CellManager::cleanup() {
    if (cellBuffer != 0) {
        glDeleteBuffers(1, &cellBuffer);
        cellBuffer = 0;
    }
    if (instanceBuffer != 0) {
        glDeleteBuffers(1, &instanceBuffer);
        instanceBuffer = 0;
    }
    if (genomeModeBuffer != 0) {
        glDeleteBuffers(1, &genomeModeBuffer);
        genomeModeBuffer = 0;
    }if (extractShader) {
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
    physicsShader->use();    // Set uniforms
    physicsShader->setFloat("u_deltaTime", deltaTime);
    physicsShader->setInt("u_cellCount", cell_count);
    physicsShader->setFloat("u_damping", 0.995f); // Reduced damping for better separation
    
    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    physicsShader->setInt("u_draggedCellIndex", draggedIndex);
    
    // Pass genome mode count
    int genomeModeCount = g_genomeSystem ? static_cast<int>(g_genomeSystem->getModeCount()) : 1;
    physicsShader->setInt("u_genomeModeCount", genomeModeCount);

    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, genomeModeBuffer);

    // Dispatch compute shader
    // Use work groups of 64 threads each
    GLuint numGroups = (cell_count + 63) / 64; // Round up division
    physicsShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runUpdateCompute(float deltaTime) {
    updateShader->use();    // Set uniforms
    updateShader->setFloat("u_deltaTime", deltaTime);
    updateShader->setInt("u_cellCount", cell_count);
    updateShader->setFloat("u_damping", 0.995f); // Reduced damping for better separation
    
    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    updateShader->setInt("u_draggedCellIndex", draggedIndex);
    
    // Pass genome mode count
    int genomeModeCount = g_genomeSystem ? static_cast<int>(g_genomeSystem->getModeCount()) : 1;
    updateShader->setInt("u_genomeModeCount", genomeModeCount);

    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, genomeModeBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cell_count + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::spawnCells(int count) {
    for (int i = 0; i < count && cell_count < config::MAX_CELLS; ++i) {
        glm::vec3 position;
        glm::vec3 velocity;
        
        if (count == 1) {
            // For single cell, spawn at origin with no initial velocity
            position = glm::vec3(0.0f, 0.0f, 0.0f);
            velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        } else {
            // For multiple cells, use random distribution
            float angle1 = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
            float angle2 = static_cast<float>(rand()) / RAND_MAX * 3.14159f;
            float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;

            position = glm::vec3(
                radius * sin(angle2) * cos(angle1),
                radius * cos(angle2),
                radius * sin(angle2) * sin(angle1)
            );

            // Random velocity
            velocity = glm::vec3(
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
                (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f
            );
        }        // Consistent mass and radius for predictable behavior
        float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        float cellRadius = 1.0f; // Consistent radius of 1.0 for all cells

        // Use initial genome mode for spawned cells
        int initialModeIndex = g_genomeSystem ? g_genomeSystem->getInitialModeIndex() : 0;
        addCell(position, velocity, mass, cellRadius, initialModeIndex);
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
        // Only update velocity to zero, don't overwrite entire cell data
        // This preserves the split timer that has been incrementing on GPU
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
        
        // Get current GPU data to preserve split timer and other state
        ComputeCell* cellData = static_cast<ComputeCell*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE));
        if (cellData) {
            // Only reset velocity, preserve everything else (especially split timer)
            cellData[selectedCell.cellIndex].velocityAndMass.x = 0.0f;
            cellData[selectedCell.cellIndex].velocityAndMass.y = 0.0f;
            cellData[selectedCell.cellIndex].velocityAndMass.z = 0.0f;
            
            // Update CPU storage with current GPU state
            cpuCells[selectedCell.cellIndex] = cellData[selectedCell.cellIndex];
            
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        }
        
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

// Genome system integration methods
void CellManager::updateGenomeModeBuffer() {
    if (!g_genomeSystem) return;
    
    const auto& gpuModes = g_genomeSystem->getGPUModes();
    if (gpuModes.empty()) return;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, genomeModeBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, 
                   gpuModes.size() * sizeof(GPUGenomeMode), 
                   gpuModes.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::onGenomeChanged() {
    // Update the GPU buffer when genome data changes
    updateGenomeModeBuffer();
    
    // Optionally update existing cells if needed
    // For now, cells will use their current mode index and pick up
    // the new genome data on their next computation cycle
}

// Cell division system implementation
void CellManager::processCellDivisions() {
    // First, sync the current GPU data to check for division flags
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellBuffer);
    ComputeCell* gpuData = static_cast<ComputeCell*>(glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE));
    
    if (!gpuData) {
        std::cerr << "Failed to map GPU buffer for division processing" << std::endl;
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
        return;
    }    // Check each cell for division flags and process them
    std::vector<std::pair<int, ComputeCell>> cellsToDiv;

    for (int i = 0; i < cell_count; ++i) {
        // Check if the division flag (bit 0) is set
        if ((gpuData[i].genomeData.w & 1) != 0) {
            // Store both the index and the current GPU data
            cellsToDiv.push_back({i, gpuData[i]});
            // Clear the division flag
            gpuData[i].genomeData.w &= ~1;
        }
    }
    
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Process divisions (create new cells) using current GPU positions
    for (const auto& divisionPair : cellsToDiv) {
        divideCellAtIndex(divisionPair.first, divisionPair.second);
    }
}

void CellManager::divideCellAtIndex(int parentIndex, const ComputeCell& currentParentData) {
    if (parentIndex < 0 || parentIndex >= cell_count) return;
    if (cell_count >= config::MAX_CELLS - 1) {
        std::cout << "Cannot divide: Maximum cell count would be exceeded" << std::endl;
        return;
    }
    
    // Use the current GPU data instead of outdated CPU data
    const ComputeCell& parent = currentParentData;
    
    // Get the genome mode for this cell
    int modeIndex = parent.genomeData.x;
    const GenomeMode* mode = nullptr;
    if (g_genomeSystem && modeIndex >= 0 && modeIndex < static_cast<int>(g_genomeSystem->getModeCount())) {
        mode = g_genomeSystem->getMode(modeIndex);
    }
    
    if (!mode) {
        std::cout << "Cannot divide: Invalid genome mode" << std::endl;
        return;
    }
    
    // Calculate division direction using parent split angles
    float yaw = glm::radians(mode->parentSplitYaw);
    float pitch = glm::radians(mode->parentSplitPitch);
    float roll = glm::radians(mode->parentSplitRoll);
    
    // Create direction vector from spherical coordinates
    glm::vec3 splitDirection = glm::vec3(
        cos(pitch) * cos(yaw),
        sin(pitch),
        cos(pitch) * sin(yaw)
    );    // Calculate positions for child cells with initial overlap
    // Position children symmetrically around the parent's center
    float separationDistance = 0.25f; // Half separation distance for each child from center
    glm::vec3 parentPos = glm::vec3(parent.positionAndRadius);
    
    // Debug: Print parent position to verify we're using current GPU data
    std::cout << "Dividing cell " << parentIndex << " at position: (" 
              << parentPos.x << ", " << parentPos.y << ", " << parentPos.z << ")" << std::endl;
    
    glm::vec3 childA_Pos = parentPos + splitDirection * separationDistance;
    glm::vec3 childB_Pos = parentPos - splitDirection * separationDistance;
      // Calculate child properties
    float childMass = parent.velocityAndMass.w * 0.5f; // Split mass
    float childRadius = 1.0f; // Consistent radius of 1.0 for all cells
      // Determine child modes
    int childA_Mode = (mode->childAModeIndex >= 0) ? mode->childAModeIndex : modeIndex;
    int childB_Mode = (mode->childBModeIndex >= 0) ? mode->childBModeIndex : modeIndex;    // Calculate separation velocity with proper momentum conservation
    // Total momentum before = parent.mass * parent.velocity
    // Total momentum after = childA.mass * childA.velocity + childB.mass * childB.velocity
    // Since both children have equal mass (0.5 * parent.mass), their velocities should be:
    // childA.velocity = parent.velocity + separation.velocity
    // childB.velocity = parent.velocity - separation.velocity
    // This ensures: childA.mass * (parent.velocity + sep) + childB.mass * (parent.velocity - sep) = parent.mass * parent.velocity
    
    float separationSpeed = 5.0f + (parent.velocityAndMass.w * 0.8f); // Increased base speed and mass factor
    glm::vec3 parentVelocity = glm::vec3(parent.velocityAndMass);
    glm::vec3 separationVelocity = splitDirection * separationSpeed;    // Create child A with positive separation velocity
    glm::vec3 childA_Velocity = parentVelocity + separationVelocity;
    addCellToStorage(childA_Pos, childA_Velocity, childMass, childRadius, childA_Mode);
    
    // Create child B with negative separation velocity (equal and opposite force)
    glm::vec3 childB_Velocity = parentVelocity - separationVelocity;
    addCellToStorage(childB_Pos, childB_Velocity, childMass, childRadius, childB_Mode);
    
    // Debug: Print momentum conservation verification
    glm::vec3 totalMomentumBefore = parent.velocityAndMass.w * parentVelocity;
    glm::vec3 totalMomentumAfter = childMass * childA_Velocity + childMass * childB_Velocity;
    std::cout << "Division momentum - Before: (" << totalMomentumBefore.x << ", " << totalMomentumBefore.y << ", " << totalMomentumBefore.z 
              << ") After: (" << totalMomentumAfter.x << ", " << totalMomentumAfter.y << ", " << totalMomentumAfter.z << ")" << std::endl;
    
    // Remove the parent cell completely
    // Check if the selected cell is the parent being removed
    if (selectedCell.isValid && selectedCell.cellIndex == parentIndex) {
        clearSelection(); // Clear selection since the cell is being removed
    } else if (selectedCell.isValid && selectedCell.cellIndex > parentIndex) {
        // If selected cell index is after the parent, adjust it since indices will shift
        selectedCell.cellIndex--;
    }
      // Remove parent from CPU storage
    cpuCells.erase(cpuCells.begin() + parentIndex);
    cell_count--;
    
    // Now update GPU buffer with all changes at once (new children + removed parent)
    updateGPUBuffers();
    
    std::cout << "Cell " << parentIndex << " divided and parent removed! New cell count: " << cell_count << std::endl;
}

void CellManager::resetSimulation(int initialCellCount) {
    std::cout << "Resetting simulation..." << std::endl;
    
    // Clear any current selection
    clearSelection();
    
    // Clear CPU storage
    cpuCells.clear();
    cell_count = 0;
    
    // Clear GPU buffers by resetting them to empty state
    updateGPUBuffers();
    
    // Spawn initial cells
    spawnCells(initialCellCount);
    
    std::cout << "Simulation reset with " << initialCellCount << " cells." << std::endl;
}