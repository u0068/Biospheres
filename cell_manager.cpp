#include "cell_manager.h"
#include "camera.h"
#include "config.h"
#include <iostream>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "timer.h"

CellManager::CellManager()
{
    // Generate sphere mesh
    sphereMesh.generateSphere(12, 16, 1.0f); // Even lower poly count: 12x16 = 192 triangles
    sphereMesh.setupBuffers();

    initializeGPUBuffers();
    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell_physics.comp");
    updateShader = new Shader("shaders/cell_update.comp");
    extractShader = new Shader("shaders/extract_instances.comp");
    
    // Initialize spatial partitioning
    initializeSpatialGrid();
}

CellManager::~CellManager()
{
    cleanup();
    cleanupSpatialGrid();
}

void CellManager::initializeGPUBuffers()
{
    // Create all the buffers that the cell manager is going to use
    allBuffers.init(
        {std::ref(positionBuffer),
         std::ref(velocityBuffer),
         std::ref(accelerationBuffer),
         std::ref(massBuffer),
         std::ref(radiusBuffer)},
        {sizeof(glm::vec4),
         sizeof(glm::vec4),
         sizeof(glm::vec4),
         sizeof(float),
         sizeof(float)},
        config::MAX_CELLS,
        true);

    instanceBuffers.init(
        {positionBuffer,
         radiusBuffer},
        {
            sizeof(glm::vec4),
            sizeof(float),
        },
        config::MAX_CELLS);

    updateBuffers.init(
        {
            positionBuffer,
            velocityBuffer,
            accelerationBuffer,
        },
        {
            sizeof(glm::vec4),
            sizeof(glm::vec4),
            sizeof(glm::vec4),
        },
        config::MAX_CELLS);

    physicsBuffers.init(
        {
            positionBuffer,
            accelerationBuffer,
            radiusBuffer,
            massBuffer,
        },
        {
            sizeof(glm::vec4),
            sizeof(glm::vec4),
            sizeof(float),
            sizeof(float),
        },
        config::MAX_CELLS);

    // Setup the sphere mesh, which needs to know the positions and radii
    sphereMesh.setupInstanceBuffer(instanceBuffers);

    // Reserve CPU storage (SoA)
    cellDataSoA.reserve(config::MAX_CELLS);
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
    {

        // Use compute shader to efficiently extract instance data
        extractShader->use();
        extractShader->setInt("u_cellCount", cellCount);

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

void CellManager::addCellsToGPUBuffer(const std::vector<ComputeCell> &cells)
{ // Prefer to not use this directly, use addCellToStagingBuffer instead
    int newCellCount = static_cast<int>(cells.size());

    std::cout << "Adding " << newCellCount << " cells to GPU buffer. Current cell count: " << cellCount + newCellCount << "\n";

    if (cellCount + newCellCount > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    TimerCPU cpuTimer("Converting CPU AoS to GPU SoA (Legacy)");

    const int count = static_cast<int>(cells.size());

    std::vector<glm::vec4> positions;
    std::vector<glm::vec4> velocities;
    std::vector<glm::vec4> accelerations;
    std::vector<float> masses;
    std::vector<float> radii;

    positions.reserve(count);
    velocities.reserve(count);
    accelerations.reserve(count);
    masses.reserve(count);
    radii.reserve(count);

    // Convert from AoS to SoA
    for (const ComputeCell &cell : cells)
    {
        positions.push_back(cell.positionAndRadius); // GPU needs vec4
        velocities.push_back(cell.velocityAndMass);
        accelerations.push_back(cell.acceleration);
        masses.push_back(cell.velocityAndMass.w);  // Extract mass
        radii.push_back(cell.positionAndRadius.w); // Extract radius
    }

    std::vector<void *> gpuData = {
        positions.data(),
        velocities.data(),
        accelerations.data(),
        masses.data(),
        radii.data()};

    TimerGPU gpuTimer("Adding Cells to GPU Buffer", true);

    // Update only the specific part of the GPU buffer
    int dstIndex = cellCount;
    allBuffers.update(dstIndex, count, gpuData);

    cellCount += newCellCount;
}

void CellManager::addCellsToGPUBufferSoA(const CellDataSoA &cellData)
{
    int newCellCount = static_cast<int>(cellData.size());

    std::cout << "Adding " << newCellCount << " cells to GPU buffer (SoA optimized). Current cell count: " << cellCount + newCellCount << "\n";

    if (cellCount + newCellCount > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    TimerCPU cpuTimer("Preparing SoA data for GPU upload");

    // Get GPU-compatible data directly from SoA structure
    auto positions = cellData.getPositionAndRadiusData();
    auto velocities = cellData.getVelocityAndMassData();
    auto accelerations = cellData.getAccelerationData();

    std::vector<void *> gpuData = {
        positions.data(),
        velocities.data(),
        accelerations.data(),
        const_cast<float *>(cellData.masses.data()),
        const_cast<float *>(cellData.radii.data())};

    TimerGPU gpuTimer("Adding Cells to GPU Buffer (SoA)", true);

    // Update only the specific part of the GPU buffer
    int dstIndex = cellCount;
    allBuffers.update(dstIndex, newCellCount, gpuData);

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

    // Add to legacy staging buffer for compatibility
    cellStagingBuffer.push_back(newCell);
    pendingCellCount++;
}

void CellManager::addCellToStagingBufferSoA(const glm::vec3 &position, const glm::vec3 &velocity,
                                            const glm::vec3 &acceleration, float mass, float radius)
{
    if (cellCount + pendingCellCount + 1 > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    // Add directly to SoA staging buffer (most efficient)
    stagingDataSoA.addCell(position, velocity, acceleration, mass, radius);
    pendingCellCount++;
}

void CellManager::addStagedCellsToGPUBuffer()
{
    // Prioritize direct SoA upload (most efficient)
    if (stagingDataSoA.size() > 0)
    {
        uploadSoADataDirectly(stagingDataSoA);
        stagingDataSoA.clear();
    }    // Handle legacy AoS staging buffer for backward compatibility
    if (!cellStagingBuffer.empty())
    {
        addCellsToGPUBuffer(cellStagingBuffer);
        cellStagingBuffer.clear();
    }

    pendingCellCount = 0; // Reset pending count
}

void CellManager::addStagedCellsToGPUBufferSoA()
{
    if (stagingDataSoA.size() > 0)
    {
        uploadSoADataDirectly(stagingDataSoA);
        stagingDataSoA.clear();
        pendingCellCount = 0;
    }
}

ComputeCell CellManager::getCellData(int index) const
{
    if (index < 0 || index >= cellCount)
    {
        return ComputeCell{}; // Return empty cell if index is invalid
    }

    // For staging data, check the staging buffer first
    if (static_cast<size_t>(index) < stagingDataSoA.size())
    {
        return stagingDataSoA.getCell(index);
    }

    // For committed data, read directly from GPU buffers (most accurate)
    ComputeCell cell;

    // Read position and radius
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    void *posData = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                     index * sizeof(glm::vec4),
                                     sizeof(glm::vec4),
                                     GL_MAP_READ_BIT);
    if (posData)
    {
        cell.positionAndRadius = *static_cast<glm::vec4 *>(posData);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    // Read velocity and mass
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
    void *velData = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                     index * sizeof(glm::vec4),
                                     sizeof(glm::vec4),
                                     GL_MAP_READ_BIT);
    if (velData)
    {
        cell.velocityAndMass = *static_cast<glm::vec4 *>(velData);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    // Read acceleration
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, accelerationBuffer);
    void *accelData = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                       index * sizeof(glm::vec4),
                                       sizeof(glm::vec4),
                                       GL_MAP_READ_BIT);
    if (accelData)
    {
        cell.acceleration = *static_cast<glm::vec4 *>(accelData);
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }

    return cell;
}

bool CellManager::getCellDataSoA(int index, glm::vec3 &position, glm::vec3 &velocity, glm::vec3 &acceleration, float &mass, float &radius) const
{
    if (index < 0 || index >= cellCount)
    {
        return false;
    }

    // For staging data, check the staging buffer first
    if (static_cast<size_t>(index) < stagingDataSoA.size())
    {
        position = stagingDataSoA.positions[index];
        velocity = stagingDataSoA.velocities[index];
        acceleration = stagingDataSoA.accelerations[index];
        mass = stagingDataSoA.masses[index];
        radius = stagingDataSoA.radii[index];
        return true;
    }

    // For committed data, read directly from GPU buffers
    // This would require GPU readback - for now, return false to indicate data not available
    // In a production system, you might implement async readback here
    return false;
}

void CellManager::updateCellDataSoA(int index, const glm::vec3 &position, const glm::vec3 &velocity, const glm::vec3 &acceleration, float mass, float radius)
{
    if (index >= 0 && static_cast<size_t>(index) < stagingDataSoA.size())
    {
        // Update staging SoA data (for uncommitted cells)
        stagingDataSoA.positions[index] = position;
        stagingDataSoA.velocities[index] = velocity;
        stagingDataSoA.accelerations[index] = acceleration;
        stagingDataSoA.masses[index] = mass;
        stagingDataSoA.radii[index] = radius;

        // Update selected cell cache if this is the selected cell
        if (selectedCell.isValid && selectedCell.cellIndex == index)
        {
            selectedCell.cellData.positionAndRadius = glm::vec4(position, radius);
            selectedCell.cellData.velocityAndMass = glm::vec4(velocity, mass);
            selectedCell.cellData.acceleration = glm::vec4(acceleration, 0.0f);
        }
    }
    else if (index >= 0 && index < cellCount)
    {
        // For committed cells, update GPU buffer directly
        glm::vec4 positionAndRadius = glm::vec4(position, radius);
        glm::vec4 velocityAndMass = glm::vec4(velocity, mass);
        glm::vec4 accelerationData = glm::vec4(acceleration, 0.0f);

        glNamedBufferSubData(positionBuffer, index * sizeof(glm::vec4), sizeof(glm::vec4), &positionAndRadius);
        glNamedBufferSubData(velocityBuffer, index * sizeof(glm::vec4), sizeof(glm::vec4), &velocityAndMass);
        glNamedBufferSubData(accelerationBuffer, index * sizeof(glm::vec4), sizeof(glm::vec4), &accelerationData);
        glNamedBufferSubData(massBuffer, index * sizeof(float), sizeof(float), &mass);
        glNamedBufferSubData(radiusBuffer, index * sizeof(float), sizeof(float), &radius);

        // Update selected cell cache
        if (selectedCell.isValid && selectedCell.cellIndex == index)
        {
            selectedCell.cellData.positionAndRadius = positionAndRadius;
            selectedCell.cellData.velocityAndMass = velocityAndMass;
            selectedCell.cellData.acceleration = accelerationData;
        }
    }
}

void CellManager::bindAllBuffers()
{ // Its safe to bind all buffers once before running shaders if we can GUARANTEE that nothing is gonna bind other stuff while they run

    TimerGPU timer("Binding Buffers");

    allBuffers.bindBase({
        0, // position
        1, // velocity
        2, // acceleration
        3, // mass
        4, // radius
    });
    
    // Bind spatial grid buffers if spatial partitioning is enabled
    if (useSpatialPartitioning) {
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gridCountsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, gridOffsetsBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, gridDataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, cellGridPosBuffer);
    }
}

void CellManager::updateCells(float deltaTime)
{
    if (pendingCellCount > 0)
    {
        addStagedCellsToGPUBuffer(); // Sync any pending cells to GPU
    }

    if (cellCount == 0)
        return;

    {
        TimerGPU timer("Cell Collision Resolution");

        // Run physics computation on GPU
        runPhysicsCompute(deltaTime);

        // Add memory barrier to ensure physics calculations are complete
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    {

        TimerGPU timer("Cell Physics Update");
        // Run position/velocity update on GPU
        runUpdateCompute(deltaTime);

        // Add memory barrier to ensure all computations are complete
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
}

void CellManager::cleanup()
{

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
    if (useSpatialPartitioning && gridPhysicsShader) {
        // Update spatial grid first
        updateSpatialGrid();
        
        // Use grid-based physics shader
        gridPhysicsShader->use();
        gridPhysicsShader->setFloat("u_deltaTime", deltaTime);
        gridPhysicsShader->setInt("u_cellCount", cellCount);
        gridPhysicsShader->setFloat("u_damping", 0.98f);
        gridPhysicsShader->setFloat("u_worldSize", SpatialGrid::WORLD_SIZE);
        gridPhysicsShader->setInt("u_gridSize", SpatialGrid::GRID_SIZE);
        gridPhysicsShader->setInt("u_maxCellsPerGrid", SpatialGrid::MAX_CELLS_PER_GRID);
        
        // Pass dragged cell index to skip its physics
        int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
        gridPhysicsShader->setInt("u_draggedCellIndex", draggedIndex);
        
        // Dispatch grid-based physics compute shader
        GLuint numGroups = (cellCount + 63) / 64;
        gridPhysicsShader->dispatch(numGroups, 1, 1);
    } else {
        // Fall back to brute force physics
        physicsShader->use();

        // Set uniforms
        physicsShader->setFloat("u_deltaTime", deltaTime);
        physicsShader->setInt("u_cellCount", cellCount);
        physicsShader->setFloat("u_damping", 0.98f);

        // Pass dragged cell index to skip its physics
        int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
        physicsShader->setInt("u_draggedCellIndex", draggedIndex);

        // Dispatch compute shader
        GLuint numGroups = (cellCount + 63) / 64; // Round up division
        physicsShader->dispatch(numGroups, 1, 1);
    }
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

    // Normally we would bind relevant buffers here but we can get away with binding everything at once before all the shaders

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);
}

void CellManager::spawnCells(int count)
{
    TimerCPU cpuTimer("Spawning Cells (SoA optimized)", true);

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
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f);

        // Random mass and radius
        float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        float cellRadius = 0.5f + static_cast<float>(rand()) / RAND_MAX * 1.0f;

        // Use SoA method directly (more efficient)
        addCellToStagingBufferSoA(position, velocity, glm::vec3(0.0f), mass, cellRadius);
    }
}

void CellManager::spawnCellsLegacy(int count)
{
    TimerCPU cpuTimer("Spawning Cells (Legacy AoS)", true);

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
            (static_cast<float>(rand()) / RAND_MAX - 0.5f) * 5.0f);

        // Random mass and radius
        float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        float cellRadius = 0.5f + static_cast<float>(rand()) / RAND_MAX * 1.0f;

        // Use legacy AoS method
        ComputeCell newCell;
        newCell.positionAndRadius = glm::vec4(position, cellRadius);
        newCell.velocityAndMass = glm::vec4(velocity, mass);
        newCell.acceleration = glm::vec4(0.0f);

        addCellToStagingBuffer(newCell);
    }
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
        // Start new selection with improved raycasting
        glm::vec3 rayOrigin = camera.getPosition();
        glm::vec3 rayDirection = calculateMouseRay(mousePos, screenSize, camera);

        // Debug: Print mouse coordinates and ray info (reduced logging)
        std::cout << "Mouse click at (" << mousePos.x << ", " << mousePos.y << ")\n";

        int selectedIndex = selectCellAtPosition(rayOrigin, rayDirection);
        if (selectedIndex >= 0)
        {
            selectedCell.cellIndex = selectedIndex;
            selectedCell.cellData = getCellData(selectedIndex);
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

int CellManager::selectCellAtPosition(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection)
{
    if (cellCount == 0)
        return -1;

    float closestDistance = FLT_MAX;
    int closestCellIndex = -1;
    int intersectionCount = 0;

    // Read position and radius data from GPU buffers
    std::vector<glm::vec4> positions(cellCount);
    std::vector<float> radii(cellCount);

    // Map GPU buffers to read current cell data
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    void *posData = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (posData)
    {
        memcpy(positions.data(), posData, cellCount * sizeof(glm::vec4));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    else
    {
        std::cerr << "Failed to map position buffer for selection" << std::endl;
        return -1;
    }

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, radiusBuffer);
    void *radData = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
    if (radData)
    {
        memcpy(radii.data(), radData, cellCount * sizeof(float));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    else
    {
        std::cerr << "Failed to map radius buffer for selection" << std::endl;
        return -1;
    }

    // Debug output for raycasting
    std::cout << "Testing " << cellCount << " cells for intersection..." << std::endl;

    // Test intersection with each cell
    for (int i = 0; i < cellCount; i++)
    {
        glm::vec3 cellPosition = glm::vec3(positions[i]);
        float cellRadius = radii[i];

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

void CellManager::dragSelectedCell(const glm::vec3 &newWorldPosition)
{
    if (!selectedCell.isValid || selectedCell.cellIndex < 0 || selectedCell.cellIndex >= cellCount)
        return;

    // Update position in GPU buffer directly
    glm::vec4 newPosAndRadius = glm::vec4(newWorldPosition, selectedCell.cellData.positionAndRadius.w);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, positionBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    selectedCell.cellIndex * sizeof(glm::vec4),
                    sizeof(glm::vec4),
                    &newPosAndRadius);

    // Clear velocity when dragging to prevent conflicts with physics
    glm::vec4 zeroVelocity = glm::vec4(0.0f, 0.0f, 0.0f, selectedCell.cellData.velocityAndMass.w);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    selectedCell.cellIndex * sizeof(glm::vec4),
                    sizeof(glm::vec4),
                    &zeroVelocity);

    // Update cached selected cell data
    selectedCell.cellData.positionAndRadius.x = newWorldPosition.x;
    selectedCell.cellData.positionAndRadius.y = newWorldPosition.y;
    selectedCell.cellData.positionAndRadius.z = newWorldPosition.z;
    selectedCell.cellData.velocityAndMass.x = 0.0f;
    selectedCell.cellData.velocityAndMass.y = 0.0f;
    selectedCell.cellData.velocityAndMass.z = 0.0f;

    // Also update SoA data if the cell exists there
    if (static_cast<size_t>(selectedCell.cellIndex) < cellDataSoA.size())
    {
        cellDataSoA.positions[selectedCell.cellIndex] = newWorldPosition;
        cellDataSoA.velocities[selectedCell.cellIndex] = glm::vec3(0.0f);
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
        glm::vec4 zeroVelocity = glm::vec4(0.0f, 0.0f, 0.0f, selectedCell.cellData.velocityAndMass.w);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, velocityBuffer);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        selectedCell.cellIndex * sizeof(glm::vec4),
                        sizeof(glm::vec4),
                        &zeroVelocity);

        // Update cached data
        selectedCell.cellData.velocityAndMass.x = 0.0f;
        selectedCell.cellData.velocityAndMass.y = 0.0f;
        selectedCell.cellData.velocityAndMass.z = 0.0f;

        // Also update SoA data if the cell exists there
        if (static_cast<size_t>(selectedCell.cellIndex) < cellDataSoA.size())
        {
            cellDataSoA.velocities[selectedCell.cellIndex] = glm::vec3(0.0f);
        }
    }

    isDraggingCell = false;
}

void CellManager::syncCellPositionsFromGPU()
{
    // This method is deprecated in favor of direct GPU buffer access
    // Cell selection now reads directly from GPU buffers when needed
    std::cout << "syncCellPositionsFromGPU called (deprecated - using direct GPU access)" << std::endl;
}

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

void CellManager::uploadSoADataDirectly(const CellDataSoA &data)
{
    int newCellCount = static_cast<int>(data.size());

    if (cellCount + newCellCount > config::MAX_CELLS)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    if (newCellCount == 0)
        return;

    TimerCPU cpuTimer("Direct SoA GPU Upload (Zero Conversion)", true);

    // Prepare data pointers for direct GPU upload (no conversions needed)
    std::vector<void *> gpuData = {
        // positions as vec4 (pad with radius)
        nullptr, // Will be filled below
        // velocities as vec4 (pad with mass)
        nullptr, // Will be filled below
        // accelerations as vec4 (pad with zero)
        nullptr, // Will be filled below
        // masses as float array
        const_cast<float *>(data.masses.data()),
        // radii as float array
        const_cast<float *>(data.radii.data())};

    // Create temporary vec4 arrays for GPU compatibility
    std::vector<glm::vec4> positionsVec4;
    std::vector<glm::vec4> velocitiesVec4;
    std::vector<glm::vec4> accelerationsVec4;

    positionsVec4.reserve(newCellCount);
    velocitiesVec4.reserve(newCellCount);
    accelerationsVec4.reserve(newCellCount);

    // Direct conversion without intermediate AoS structure
    for (int i = 0; i < newCellCount; ++i)
    {
        positionsVec4.emplace_back(data.positions[i], data.radii[i]);
        velocitiesVec4.emplace_back(data.velocities[i], data.masses[i]);
        accelerationsVec4.emplace_back(data.accelerations[i], 0.0f);
    }

    gpuData[0] = positionsVec4.data();
    gpuData[1] = velocitiesVec4.data();
    gpuData[2] = accelerationsVec4.data();

    TimerGPU gpuTimer("Direct SoA GPU Buffer Update", true);

    // Update GPU buffer directly
    int dstIndex = cellCount;
    allBuffers.update(dstIndex, newCellCount, gpuData);

    cellCount += newCellCount;

    std::cout << "Uploaded " << newCellCount << " cells directly from SoA. Total cells: " << cellCount << "\n";
}

// Spatial Partitioning Implementation

void CellManager::initializeSpatialGrid()
{
    // Initialize spatial partitioning compute shaders
    gridBuildShader = new Shader("shaders/grid_build.comp");
    gridPhysicsShader = new Shader("shaders/grid_physics.comp");
    
    // Create spatial grid buffers
    glGenBuffers(1, &gridCountsBuffer);
    glGenBuffers(1, &gridOffsetsBuffer);
    glGenBuffers(1, &gridDataBuffer);
    glGenBuffers(1, &cellGridPosBuffer);
    
    // Initialize grid counts buffer (one count per grid cell)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gridCountsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 SpatialGrid::TOTAL_GRID_CELLS * sizeof(int), 
                 nullptr, GL_DYNAMIC_DRAW);
    
    // Initialize grid offsets buffer (one offset per grid cell)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gridOffsetsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 SpatialGrid::TOTAL_GRID_CELLS * sizeof(int), 
                 nullptr, GL_DYNAMIC_DRAW);
    
    // Initialize grid data buffer (stores cell indices)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gridDataBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 SpatialGrid::MAX_GRID_ENTRIES * sizeof(int), 
                 nullptr, GL_DYNAMIC_DRAW);
    
    // Initialize cell grid position buffer (grid position for each cell)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cellGridPosBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 config::MAX_CELLS * sizeof(glm::ivec4), 
                 nullptr, GL_DYNAMIC_DRAW);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    std::cout << "Spatial partitioning initialized with " << SpatialGrid::GRID_SIZE 
              << "x" << SpatialGrid::GRID_SIZE << "x" << SpatialGrid::GRID_SIZE 
              << " grid (" << SpatialGrid::TOTAL_GRID_CELLS << " cells)\n";
}

void CellManager::updateSpatialGrid()
{
    if (!useSpatialPartitioning || cellCount == 0) return;
    
    // Bind spatial grid buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, gridCountsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 6, gridOffsetsBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, gridDataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, cellGridPosBuffer);
    
    // Clear grid counts
    int zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, gridCountsBuffer);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER, GL_R32I, GL_RED_INTEGER, GL_INT, &zero);
    
    // Use grid build shader to populate spatial grid
    gridBuildShader->use();
    gridBuildShader->setInt("u_cellCount", cellCount);
    gridBuildShader->setFloat("u_worldSize", SpatialGrid::WORLD_SIZE);
    gridBuildShader->setInt("u_gridSize", SpatialGrid::GRID_SIZE);
    gridBuildShader->setInt("u_maxCellsPerGrid", SpatialGrid::MAX_CELLS_PER_GRID);
    
    // Dispatch grid build compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    gridBuildShader->dispatch(numGroups, 1, 1);
    
    // Memory barrier to ensure grid is built before physics
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::cleanupSpatialGrid()
{
    if (gridBuildShader) {
        delete gridBuildShader;
        gridBuildShader = nullptr;
    }
    if (gridPhysicsShader) {
        delete gridPhysicsShader;
        gridPhysicsShader = nullptr;
    }
    
    if (gridCountsBuffer) {
        glDeleteBuffers(1, &gridCountsBuffer);
        gridCountsBuffer = 0;
    }
    if (gridOffsetsBuffer) {
        glDeleteBuffers(1, &gridOffsetsBuffer);
        gridOffsetsBuffer = 0;
    }
    if (gridDataBuffer) {
        glDeleteBuffers(1, &gridDataBuffer);
        gridDataBuffer = 0;
    }
    if (cellGridPosBuffer) {
        glDeleteBuffers(1, &cellGridPosBuffer);
        cellGridPosBuffer = 0;
    }
}
