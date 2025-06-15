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
    initializeSpatialGrid();

    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell_physics_spatial.comp"); // Use spatial partitioning version
    updateShader = new Shader("shaders/cell_update.comp");
    extractShader = new Shader("shaders/extract_instances.comp");

    // Initialize spatial grid shaders
    gridClearShader = new Shader("shaders/grid_clear.comp");
    gridAssignShader = new Shader("shaders/grid_assign.comp");
    gridPrefixSumShader = new Shader("shaders/grid_prefix_sum.comp");
    gridInsertShader = new Shader("shaders/grid_insert.comp");
}

CellManager::~CellManager()
{
    cleanup();
}

void CellManager::cleanup()
{
    // Clean up double buffered cell buffers
    for (int i = 0; i < 2; i++)
    {
        if (cellBuffer[i] != 0)
        {
            glDeleteBuffers(1, &cellBuffer[i]);
            cellBuffer[i] = 0;
        }
        if (instanceBuffer[i] != 0)
        {
            glDeleteBuffers(1, &instanceBuffer[i]);
            instanceBuffer[i] = 0;
        }
    }

    cleanupSpatialGrid();

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

    // Cleanup spatial grid shaders
    if (gridClearShader)
    {
        gridClearShader->destroy();
        delete gridClearShader;
        gridClearShader = nullptr;
    }
    if (gridAssignShader)
    {
        gridAssignShader->destroy();
        delete gridAssignShader;
        gridAssignShader = nullptr;
    }
    if (gridPrefixSumShader)
    {
        gridPrefixSumShader->destroy();
        delete gridPrefixSumShader;
        gridPrefixSumShader = nullptr;
    }
    if (gridInsertShader)
    {
        gridInsertShader->destroy();
        delete gridInsertShader;
        gridInsertShader = nullptr;
    }

    sphereMesh.cleanup();
}

// Buffers

void CellManager::initializeGPUBuffers()
{
    // Create double buffered compute buffers for cell data
    for (int i = 0; i < 2; i++)
    {
        glCreateBuffers(1, &cellBuffer[i]);
        glNamedBufferData(cellBuffer[i], config::MAX_CELLS * sizeof(ComputeCell), nullptr, GL_DYNAMIC_DRAW);

        // Create double buffered instance buffers for rendering (contains position + radius)
        glCreateBuffers(1, &instanceBuffer[i]);
        glNamedBufferData(instanceBuffer[i], config::MAX_CELLS * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
    }

    // Setup the sphere mesh to use our current instance buffer
    sphereMesh.setupInstanceBuffer(getCurrentInstanceBuffer());

    // Reserve CPU storage
    cpuCells.reserve(config::MAX_CELLS);
}

void CellManager::addCellsToGPUBuffer(const std::vector<ComputeCell> &cells)
{ // Prefer to not use this directly, use addCellToStagingBuffer instead
    int newCellCount = cells.size();

    std::cout << "Adding " << newCellCount << " cells to GPU buffer. Current cell count: " << cellCount + newCellCount << "\n";

    if (cellCount + newCellCount > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    TimerGPU gpuTimer("Adding Cells to GPU Buffer", true);

    // Update both cell buffers to keep them synchronized
    for (int i = 0; i < 2; i++)
    {
        glNamedBufferSubData(cellBuffer[i],
                             cellCount * sizeof(ComputeCell),
                             newCellCount * sizeof(ComputeCell),
                             cells.data());
    }

    cellCount += newCellCount;
}

void CellManager::addCellToGPUBuffer(const ComputeCell &newCell)
{ // Prefer to not use this directly, use addCellToStagingBuffer instead
    addCellsToGPUBuffer({newCell});
}

void CellManager::addCellToStagingBuffer(const ComputeCell &newCell)
{
    if (cellCount + 1 > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    // Create a copy of the cell and enforce radius = 1.0f
    ComputeCell correctedCell = newCell;
    correctedCell.positionAndRadius.w = 1.0f; // Force all cells to have radius of 1

    // Add to CPU storage only (no immediate GPU sync)
    cellStagingBuffer.push_back(correctedCell);
    cpuCells.push_back(correctedCell);
    pendingCellCount++;
}

void CellManager::addStagedCellsToGPUBuffer()
{
    addCellsToGPUBuffer(cellStagingBuffer);
    cellStagingBuffer.clear(); // Clear after adding to GPU buffer
    pendingCellCount = 0;      // Reset pending count
}

ComputeCell CellManager::getCellData(int index) const
{
    if (index >= 0 && index < cellCount)
    {
        return cpuCells[index];
    }
    return ComputeCell{}; // Return empty cell if index is invalid
}

void CellManager::updateCellData(int index, const ComputeCell &newData)
{
    if (index >= 0 && index < cellCount)
    {
        // Create a copy of the new data and enforce radius = 1.0f
        ComputeCell correctedData = newData;
        correctedData.positionAndRadius.w = 1.0f; // Force all cells to have radius of 1

        cpuCells[index] = correctedData;

        // Update selected cell cache if this is the selected cell
        if (selectedCell.isValid && selectedCell.cellIndex == index)
        {
            selectedCell.cellData = correctedData;
        }

        // Update the specific cell in both GPU buffers to keep them synchronized
        for (int i = 0; i < 2; i++)
        {
            glNamedBufferSubData(cellBuffer[i],
                                 index * sizeof(ComputeCell),
                                 sizeof(ComputeCell),
                                 &cpuCells[index]);
        }
    }
}

// Cell Update

void CellManager::updateCells(float deltaTime)
{
    if (pendingCellCount > 0)
    {
        addStagedCellsToGPUBuffer(); // Sync any pending cells to GPU
    }

    if (cellCount == 0)
        return; // Update spatial grid before physics
    updateSpatialGrid();

    // Run physics computation on GPU (reads from previous, writes to current)
    runPhysicsCompute(deltaTime);

    // Run position/velocity update on GPU (still working on current buffer)
    runUpdateCompute(deltaTime);

    // Swap buffers for next frame (current becomes previous, previous becomes current)
    swapBuffers();
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
    { // Use compute shader to efficiently extract instance data
        extractShader->use();
        extractShader->setInt("u_cellCount", cellCount);

        // Bind current buffers for compute shader (read from current cell buffer, write to current instance buffer)
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCurrentCellBuffer());
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCurrentInstanceBuffer()); // Dispatch extract compute shader
        GLuint numGroups = (cellCount + 63) / 64;                                  // 64 threads per group
        extractShader->dispatch(numGroups, 1, 1);

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

        // Set selection highlighting uniforms
        if (selectedCell.isValid)
        {
            glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndRadius);
            float selectedRadius = selectedCell.cellData.positionAndRadius.w;
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

void CellManager::runPhysicsCompute(float deltaTime)
{
    TimerGPU timer("Cell Physics Compute");

    physicsShader->use();

    // Set uniforms
    physicsShader->setFloat("u_deltaTime", deltaTime);
    physicsShader->setInt("u_cellCount", cellCount);
    physicsShader->setFloat("u_damping", 0.98f);

    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    physicsShader->setInt("u_draggedCellIndex", draggedIndex);

    // Set spatial grid uniforms
    physicsShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    physicsShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    physicsShader->setFloat("u_worldSize", config::WORLD_SIZE);
    physicsShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID); // Bind buffers (read from previous buffer, write to current buffer for stable simulation)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getPreviousCellBuffer()); // Read from previous frame
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCurrentGridBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, getCurrentGridCountBuffer());

    // Also bind current buffer as output for physics results
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, getCurrentCellBuffer()); // Write to current frame

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    physicsShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runUpdateCompute(float deltaTime)
{
    TimerGPU timer("Cell Update Compute");

    updateShader->use();

    // Set uniforms
    updateShader->setFloat("u_deltaTime", deltaTime);
    updateShader->setInt("u_cellCount", cellCount);
    updateShader->setFloat("u_damping", 0.98f);

    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    updateShader->setInt("u_draggedCellIndex", draggedIndex); // Bind current cell buffer for in-place updates
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCurrentCellBuffer());

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::spawnCells(int count)
{
    TimerCPU cpuTimer("Spawning Cells", true);

    for (int i = 0; i < count && cellCount < config::MAX_CELLS; ++i)
    {
        // Random position within spawn radius
        float angle1 = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float angle2 = static_cast<float>(rand()) / RAND_MAX * 3.14159f;
        float radius = static_cast<float>(rand()) / RAND_MAX * spawnRadius;

        glm::vec3 position = glm::vec3(
            radius * sin(angle2) * cos(angle1),
            radius * cos(angle2),
            radius * sin(angle2) * sin(angle1));

        // Random velocity
        glm::vec3 velocity = glm::vec3(
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f,
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f); // Random mass and fixed radius of 1
        float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        float cellRadius = 1.0f; // All cells have the same radius of 1

        ComputeCell newCell;
        newCell.positionAndRadius = glm::vec4(position, cellRadius);
        newCell.velocityAndMass = glm::vec4(velocity, mass);
        newCell.acceleration = glm::vec4(0.0f); // Reset acceleration

        addCellToStagingBuffer(newCell);
    }
}

// Spatial partitioning
void CellManager::initializeSpatialGrid()
{
    // Create double buffered grid buffers to store cell indices
    for (int i = 0; i < 2; i++)
    {
        glCreateBuffers(1, &gridBuffer[i]);
        glNamedBufferData(gridBuffer[i],
                          config::TOTAL_GRID_CELLS * config::MAX_CELLS_PER_GRID * sizeof(GLuint),
                          nullptr, GL_DYNAMIC_DRAW);

        // Create double buffered grid count buffers to store number of cells per grid cell
        glCreateBuffers(1, &gridCountBuffer[i]);
        glNamedBufferData(gridCountBuffer[i],
                          config::TOTAL_GRID_CELLS * sizeof(GLuint),
                          nullptr, GL_DYNAMIC_DRAW);

        // Create double buffered grid offset buffers for prefix sum calculations
        glCreateBuffers(1, &gridOffsetBuffer[i]);
        glNamedBufferData(gridOffsetBuffer[i],
                          config::TOTAL_GRID_CELLS * sizeof(GLuint),
                          nullptr, GL_DYNAMIC_DRAW);
    }

    std::cout << "Initialized double buffered spatial grid with " << config::TOTAL_GRID_CELLS
              << " grid cells (" << config::GRID_RESOLUTION << "^3)\n";
    std::cout << "Grid cell size: " << config::GRID_CELL_SIZE << "\n";
    std::cout << "Max cells per grid: " << config::MAX_CELLS_PER_GRID << "\n";
}

void CellManager::updateSpatialGrid()
{
    if (cellCount == 0)
        return;
    TimerGPU timer("Spatial Grid Update");

    // Step 1: Clear grid counts
    runGridClear();

    // Step 2: Count cells per grid cell
    runGridAssign();

    // Step 3: Calculate prefix sum for offsets
    runGridPrefixSum();

    // Step 4: Insert cells into grid
    runGridInsert();
}

void CellManager::cleanupSpatialGrid()
{
    // Clean up double buffered spatial grid buffers
    for (int i = 0; i < 2; i++)
    {
        if (gridBuffer[i] != 0)
        {
            glDeleteBuffers(1, &gridBuffer[i]);
            gridBuffer[i] = 0;
        }
        if (gridCountBuffer[i] != 0)
        {
            glDeleteBuffers(1, &gridCountBuffer[i]);
            gridCountBuffer[i] = 0;
        }
        if (gridOffsetBuffer[i] != 0)
        {
            glDeleteBuffers(1, &gridOffsetBuffer[i]);
            gridOffsetBuffer[i] = 0;
        }
    }
}

void CellManager::runGridClear()
{
    gridClearShader->use();

    gridClearShader->setInt("u_totalGridCells", config::TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCurrentGridCountBuffer());

    GLuint numGroups = (config::TOTAL_GRID_CELLS + 63) / 64;
    gridClearShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridAssign()
{
    gridAssignShader->use();

    gridAssignShader->setInt("u_cellCount", cellCount);
    gridAssignShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    gridAssignShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    gridAssignShader->setFloat("u_worldSize", config::WORLD_SIZE);

    // Use previous buffer for spatial grid to match physics compute input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getPreviousCellBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCurrentGridCountBuffer());

    GLuint numGroups = (cellCount + 63) / 64;
    gridAssignShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridPrefixSum()
{
    gridPrefixSumShader->use();

    gridPrefixSumShader->setInt("u_totalGridCells", config::TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCurrentGridCountBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCurrentGridOffsetBuffer());

    GLuint numGroups = (config::TOTAL_GRID_CELLS + 63) / 64;
    gridPrefixSumShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridInsert()
{
    gridInsertShader->use();

    gridInsertShader->setInt("u_cellCount", cellCount);
    gridInsertShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    gridInsertShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    gridInsertShader->setFloat("u_worldSize", config::WORLD_SIZE);
    gridInsertShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID); // Use previous buffer for spatial grid to match physics compute input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getPreviousCellBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCurrentGridBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, getCurrentGridOffsetBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, getCurrentGridCountBuffer());

    GLuint numGroups = (cellCount + 63) / 64;
    gridInsertShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
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
        glm::vec3 cellPosition = glm::vec3(cpuCells[i].positionAndRadius);
        float cellRadius = cpuCells[i].positionAndRadius.w;

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

    // Update CPU cell data
    cpuCells[selectedCell.cellIndex].positionAndRadius.x = newWorldPosition.x;
    cpuCells[selectedCell.cellIndex].positionAndRadius.y = newWorldPosition.y;
    cpuCells[selectedCell.cellIndex].positionAndRadius.z = newWorldPosition.z;

    // Clear velocity when dragging to prevent conflicts with physics
    cpuCells[selectedCell.cellIndex].velocityAndMass.x = 0.0f;
    cpuCells[selectedCell.cellIndex].velocityAndMass.y = 0.0f;
    cpuCells[selectedCell.cellIndex].velocityAndMass.z = 0.0f; // Update cached selected cell data
    selectedCell.cellData = cpuCells[selectedCell.cellIndex];

    // Update GPU buffers immediately to ensure compute shaders see the new position
    for (int i = 0; i < 2; i++)
    {
        glNamedBufferSubData(cellBuffer[i],
                             selectedCell.cellIndex * sizeof(ComputeCell),
                             sizeof(ComputeCell),
                             &cpuCells[selectedCell.cellIndex]);
    }
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
        cpuCells[selectedCell.cellIndex].velocityAndMass.x = 0.0f;
        cpuCells[selectedCell.cellIndex].velocityAndMass.y = 0.0f;
        cpuCells[selectedCell.cellIndex].velocityAndMass.z = 0.0f; // Update the GPU buffers with the final state
        for (int i = 0; i < 2; i++)
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
    if (cellCount == 0)
        return;

    // Use glMapBuffer for efficient GPU->CPU data transfer from current buffer
    ComputeCell *gpuData = static_cast<ComputeCell *>(glMapNamedBuffer(getCurrentCellBuffer(), GL_READ_ONLY));

    if (gpuData)
    {
        // Copy only the position data from GPU to CPU (don't overwrite velocity/mass as those might be needed)
        for (int i = 0; i < cellCount; i++)
        {
            // Only sync position and radius, preserve velocity and mass from CPU
            cpuCells[i].positionAndRadius = gpuData[i].positionAndRadius;
        }

        glUnmapNamedBuffer(getCurrentCellBuffer());

        std::cout << "Synced " << cellCount << " cell positions from GPU" << std::endl;
    }
    else
    {
        std::cerr << "Failed to map GPU buffer for readback" << std::endl;
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

void CellManager::swapBuffers()
{
    // Swap buffer indices for double buffering
    currentBufferIndex = 1 - currentBufferIndex;
    previousBufferIndex = 1 - previousBufferIndex;

    // Update the sphere mesh to use the new current instance buffer
    sphereMesh.setupInstanceBuffer(getCurrentInstanceBuffer());
}
