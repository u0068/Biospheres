#include "cell_manager.h"
#include "camera.h"
#include "config.h"
#include "ui_manager.h"
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

#include "genome.h"
#include "timer.h"

// ——— simple "delta" rotation around a local axis ———
static void applyLocalRotation(glm::quat &q, const glm::vec3 &axis, float deltaDeg) {
    // build a tiny rotation of deltaDeg° about `axis`
    glm::quat d = glm::angleAxis(glm::radians(deltaDeg), axis);
    // apply it in LOCAL space:
    q = glm::normalize(q * d);
}

CellManager::CellManager()
{
    // Generate sphere mesh - optimized for high cell counts
    sphereMesh.generateSphere(8, 12, 1.0f); // Ultra-low poly: 8x12 = 96 triangles for maximum performance
    sphereMesh.setupBuffers();

    initializeGPUBuffers();
    initializeSpatialGrid();
    initializeIDSystem();

    // Initialize compute shaders
    physicsShader = new Shader("shaders/cell_physics_spatial.comp"); // Use spatial partitioning version
    updateShader = new Shader("shaders/cell_update.comp");
    internalUpdateShader = new Shader("shaders/cell_update_internal.comp");
    extractShader = new Shader("shaders/extract_instances.comp");
    cellCounterShader = new Shader("shaders/cell_counter.comp");
    cellAdditionShader = new Shader("shaders/apply_additions.comp");
    idManagerShader = new Shader("shaders/id_manager.comp");

    // Initialize spatial grid shaders
    gridClearShader = new Shader("shaders/grid_clear.comp");
    gridAssignShader = new Shader("shaders/grid_assign.comp");
    gridPrefixSumShader = new Shader("shaders/grid_prefix_sum.comp");
    gridInsertShader = new Shader("shaders/grid_insert.comp");
    
    // Initialize gizmo shaders
    gizmoExtractShader = new Shader("shaders/gizmo_extract.comp");
    gizmoShader = new Shader("shaders/gizmo.vert", "shaders/gizmo.frag");
    
    // Initialize ring gizmo shaders
    ringGizmoExtractShader = new Shader("shaders/ring_gizmo_extract.comp");
    ringGizmoShader = new Shader("shaders/ring_gizmo.vert", "shaders/ring_gizmo.frag");
    
    // Initialize adhesion line shaders
    adhesionLineExtractShader = new Shader("shaders/adhesion_line_extract.comp");
    adhesionLineShader = new Shader("shaders/adhesion_line.vert", "shaders/adhesion_line.frag");
    
    // Initialize gizmo buffers
    initializeGizmoBuffers();
    initializeRingGizmoBuffers();
    initializeAdhesionLineBuffers();
    
    // Initialize LOD system
    initializeLODSystem();
    
    // Initialize barrier optimization system
    barrierBatch.setStats(&barrierStats);
}

CellManager::~CellManager()
{
    cleanup();
}

void CellManager::cleanup()
{
    // Clean up triple buffered cell buffers
    for (int i = 0; i < 3; i++)
    {
        if (cellBuffer[i] != 0)
        {
            glDeleteBuffers(1, &cellBuffer[i]);
            cellBuffer[i] = 0;
        }
    }
    if (instanceBuffer != 0)
    {
        glDeleteBuffers(1, &instanceBuffer);
        instanceBuffer = 0;
    }
    if (modeBuffer != 0)
    {
        glDeleteBuffers(1, &modeBuffer);
        modeBuffer = 0;
    }
    if (gpuCellCountBuffer != 0)
    {
        glDeleteBuffers(1, &gpuCellCountBuffer);
        gpuCellCountBuffer = 0;
    }
    if (stagingCellCountBuffer != 0)
    {
        glDeleteBuffers(1, &stagingCellCountBuffer);
        stagingCellCountBuffer = 0;
    }
    if (stagingCellBuffer != 0)
    {
        glDeleteBuffers(1, &stagingCellBuffer);
        stagingCellBuffer = 0;
    }
    if (cellAdditionBuffer != 0)
    {
        glDeleteBuffers(1, &cellAdditionBuffer);
        cellAdditionBuffer = 0;
    }

    cleanupSpatialGrid();
    cleanupIDSystem();
    cleanupLODSystem();

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
    if (idManagerShader)
    {
        idManagerShader->destroy();
        delete idManagerShader;
        idManagerShader = nullptr;
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
    
    // Cleanup gizmo shaders
    if (gizmoExtractShader)
    {
        gizmoExtractShader->destroy();
        delete gizmoExtractShader;
        gizmoExtractShader = nullptr;
    }
    if (gizmoShader)
    {
        gizmoShader->destroy();
        delete gizmoShader;
        gizmoShader = nullptr;
    }
    if (ringGizmoExtractShader)
    {
        ringGizmoExtractShader->destroy();
        delete ringGizmoExtractShader;
        ringGizmoExtractShader = nullptr;
    }
    if (ringGizmoShader)
    {
        ringGizmoShader->destroy();
        delete ringGizmoShader;
        ringGizmoShader = nullptr;
    }
    if (adhesionLineExtractShader)
    {
        adhesionLineExtractShader->destroy();
        delete adhesionLineExtractShader;
        adhesionLineExtractShader = nullptr;
    }
    if (adhesionLineShader)
    {
        adhesionLineShader->destroy();
        delete adhesionLineShader;
        adhesionLineShader = nullptr;
    }
    
    cleanupGizmos();
    cleanupRingGizmos();
    cleanupAdhesionLines();
    sphereMesh.cleanup();
}

// Buffers

void CellManager::initializeGPUBuffers()
{    // Create triple buffered compute buffers for cell data
    for (int i = 0; i < 3; i++)
    {
        glCreateBuffers(1, &cellBuffer[i]);
        glNamedBufferData(
            cellBuffer[i],
            cellLimit * sizeof(ComputeCell),
            nullptr,
            GL_DYNAMIC_COPY  // Used by both GPU compute and CPU read operations
        );

    }

    // Create instance buffer for rendering (contains position + radius + color + orientation)
    glCreateBuffers(1, &instanceBuffer);
    glNamedBufferData(
        instanceBuffer,
        cellLimit * sizeof(glm::vec4) * 3, // 3 vec4s: positionAndRadius, color, orientation
        nullptr,
        GL_DYNAMIC_COPY  // GPU produces data, GPU consumes for rendering
    );

    // Create single buffered genome buffer
    glCreateBuffers(1, &modeBuffer);
    glNamedBufferData(modeBuffer,
        cellLimit * sizeof(GPUMode),
        nullptr,
        GL_DYNAMIC_COPY  // Written once by CPU, read frequently by GPU compute shaders
    );

    // A buffer that keeps track of how many cells there are in the simulation
    glCreateBuffers(1, &gpuCellCountBuffer);
    glNamedBufferStorage(
        gpuCellCountBuffer,
        sizeof(GLuint) * 2, // stores current cell count and pending cell count, can be expanded later to also include other counts
        nullptr,
        GL_DYNAMIC_STORAGE_BIT
    );

    glCreateBuffers(1, &stagingCellCountBuffer);
    glNamedBufferStorage(
        stagingCellCountBuffer,
        sizeof(GLuint) * 2,
        nullptr,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
    );
    mappedPtr = glMapNamedBufferRange(stagingCellCountBuffer, 0, sizeof(GLuint) * 2,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    countPtr = static_cast<GLuint*>(mappedPtr);

    // Cell data staging buffer for CPU reads (avoids GPU->CPU transfer warnings)
    glCreateBuffers(1, &stagingCellBuffer);
    glNamedBufferStorage(
        stagingCellBuffer,
        cellLimit * sizeof(ComputeCell),
        nullptr,
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT
    );
    mappedCellPtr = glMapNamedBufferRange(stagingCellBuffer, 0, cellLimit * sizeof(ComputeCell),
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);

    // Cell addition queue buffer
    glCreateBuffers(1, &cellAdditionBuffer);
    glNamedBufferData(cellAdditionBuffer,
        cellLimit * sizeof(ComputeCell) / 2, // Worst case scenario
        nullptr,
        GL_STREAM_COPY  // Frequently updated by GPU compute shaders
    );

    // Setup the sphere mesh to use our current instance buffer
    sphereMesh.setupInstanceBuffer(instanceBuffer);

    // Reserve CPU storage
    cpuCells.reserve(cellLimit);
}

void CellManager::addCellsToGPUBuffer(const std::vector<ComputeCell> &cells)
{ // Prefer to not use this directly, use addCellToStagingBuffer instead
    int newCellCount = static_cast<int>(cells.size());



    if (cellCount + newCellCount > cellLimit)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }


    TimerGPU gpuTimer("Adding Cells to GPU Buffers");
    // Update both cell buffers to keep them synchronized
    //for (int i = 0; i < 2; i++)
    //{
    //    glNamedBufferSubData(cellBuffer[i],
    //                         cellCount * sizeof(ComputeCell),
    //                         newCellCount * sizeof(ComputeCell),
    //                         cells.data());
    //}

    glNamedBufferSubData(cellAdditionBuffer,
        gpuPendingCellCount * sizeof(ComputeCell),
        newCellCount * sizeof(ComputeCell),
        cells.data());

    gpuPendingCellCount += newCellCount;
    glNamedBufferSubData(gpuCellCountBuffer, sizeof(int), sizeof(int), &gpuPendingCellCount);
    glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint) * 2);

    //cellCount += newCellCount;    // Cell count should be kept track of by the counter on the gpu
}

void CellManager::addCellToGPUBuffer(const ComputeCell &newCell)
{ // Prefer to not use this directly, use addCellToStagingBuffer instead
    addCellsToGPUBuffer({newCell});
}

void CellManager::addCellToStagingBuffer(const ComputeCell &newCell)
{
    if (cellCount + 1 > cellLimit)
    {
        std::cout << "Warning: Maximum cell count reached!\n";
        return;
    }

    // Create a copy of the cell and enforce radius = 1.0f
    ComputeCell correctedCell = newCell;
    correctedCell.positionAndMass.w = 1.0f; // Force all cells to have radius of 1

    // Add to CPU storage only (no immediate GPU sync)
    cellStagingBuffer.push_back(correctedCell);
    cpuCells.push_back(correctedCell);
    cpuPendingCellCount++;
}

void CellManager::addStagedCellsToGPUBuffer()
{
    if (!cellStagingBuffer.empty()) {
        addCellsToGPUBuffer(cellStagingBuffer);
        cellStagingBuffer.clear(); // Clear after adding to GPU buffer
        cpuPendingCellCount = 0;      // Reset pending count
    }
}

glm::vec3 pitchYawToVec3(float pitch, float yaw) {
    return glm::vec3(
        cos(pitch) * sin(yaw),
        sin(pitch),
        cos(pitch) * cos(yaw)
    );
}

void CellManager::addGenomeToBuffer(GenomeData& genomeData) const {
    int genomeBaseOffset = 0; // Later make it add to the end of the buffer
    int modeCount = static_cast<int>(genomeData.modes.size());

    std::vector<GPUMode> gpuModes;
    gpuModes.reserve(modeCount);

    for (size_t i = 0; i < modeCount; ++i) {
        const ModeSettings& mode = genomeData.modes[i];

        GPUMode gmode{};
        gmode.color = glm::vec4(mode.color, 0.0);
        gmode.splitInterval = mode.splitInterval;
        gmode.genomeOffset = genomeBaseOffset;

        // Convert from pitch and yaw to padded vec4
        gmode.splitDirection = glm::vec4(pitchYawToVec3(
            glm::radians(mode.parentSplitDirection.x), glm::radians(mode.parentSplitDirection.y)), 0.);

        // Store child mode indices
        gmode.childModes = glm::ivec2(mode.childA.modeNumber, mode.childB.modeNumber);

        // Directly store quaternions (no conversion)
        gmode.orientationA = mode.childA.orientation;  // now a quat already
        gmode.orientationB = mode.childB.orientation;
        
        // Store adhesion flag
        gmode.parentMakeAdhesion = mode.parentMakeAdhesion ? 1 : 0;

        gpuModes.push_back(gmode);
    }

    glNamedBufferSubData(
        modeBuffer,
        genomeBaseOffset,
        modeCount * sizeof(GPUMode),
        gpuModes.data()
    );
}

ComputeCell CellManager::getCellData(int index) const
{
    if (index >= 0 && index < cellCount && index < static_cast<int>(cpuCells.size()))
    {
        return cpuCells[index];
    }
    ComputeCell emptyCell{};
    emptyCell.setUniqueID(0, 0, 0); // Ensure empty cell has valid ID
    return emptyCell; // Return empty cell if index is invalid
}

void CellManager::updateCellData(int index, const ComputeCell &newData)
{
    if (index >= 0 && index < cellCount)
    {
        cpuCells[index] = newData;

        // Update selected cell cache if this is the selected cell
        if (selectedCell.isValid && selectedCell.cellIndex == index)
        {
            selectedCell.cellData = newData;
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
    // Clear any pending barriers from previous frame
    clearBarriers();
    
    glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint) * 2);
    
    // Add buffer update barrier but don't flush yet
    addBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);

    cellCount = countPtr[0];
    gpuPendingCellCount = countPtr[1]; // This is effectively more of a bool than an int due to its horrific inaccuracy, but that's good enough for us
    int previousCellCount = cellCount;

    if (cpuPendingCellCount > 0)
    {
        addStagedCellsToGPUBuffer(); // Sync any pending cells to GPU
    }

    if (cellCount > 0) // Don't update cells if there are no cells to update
    {
        // Flush barriers before starting compute pipeline
        flushBarriers();
        
        // OPTIMIZED: Batch all simulation compute operations
        // Update spatial grid before physics
        updateSpatialGrid(); // This handles its own barriers internally
        
        // Run physics computation on GPU (reads from previous, writes to current)
        runPhysicsCompute(deltaTime);
        
        // Run position/velocity update on GPU (still working on current buffer)
        runUpdateCompute(deltaTime);
        
        // Run cells' internal calculations (this creates new pending cells from mitosis)
        runInternalUpdateCompute(deltaTime);
        
        // Single barrier after all simulation compute operations
        addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Optimized: Only apply cell additions when actually needed
    // Use a frame counter to reduce frequency of cell addition checks
    static int frameCounter = 0;
    if (++frameCounter % 4 == 0 || gpuPendingCellCount > 0) { // Check every 4 frames or when pending
        flushBarriers(); // Ensure previous operations complete
        
        // Copy cell count data for addition check (non-blocking)
        glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint) * 2);
        
        // Use targeted barrier instead of expensive fence sync
        addBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
        flushBarriers();
        
        gpuPendingCellCount = countPtr[1];
        
        if (gpuPendingCellCount > 0) {
            applyCellAdditions(); // Apply mitosis results
            addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        }
    }

    // Run ID manager to recycle dead cell IDs (less frequently)
    if (frameCounter % 8 == 0) { // Only every 8 frames for ID management
        flushBarriers(); // Ensure previous operations complete
        runIDManager();
        addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    // Final barrier flush and update cell count
    flushBarriers();
    cellCount = countPtr[0];

    // Swap buffers for next frame (current becomes previous, previous becomes current)
    rotateBuffers();
}

void CellManager::runCellCounter()  // This only count active cells in the gpu, not cells pending addition
{
    TimerGPU timer("Cell Counter");

    // Reset cell count to 0
    glClearNamedBufferData(
        gpuCellCountBuffer,        // Our cell counter buffer
        GL_R32UI,                  // Format (match the internal type)
        GL_RED_INTEGER,            // Format layout
        GL_UNSIGNED_INT,           // Data type
        nullptr                    // Null = fill with zero
    );

    cellCounterShader->use();

    // Set uniforms
    cellCounterShader->setInt("u_maxCells", cellLimit);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gpuCellCountBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cellLimit + 63) / 64; // Round up division
    cellCounterShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // Use optimized barrier for cell counter
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers(); // Ensure writes are visible
    glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint));
}

void CellManager::renderCells(glm::vec2 resolution, Shader &cellShader, Camera &camera)
{
    // Use LOD system if enabled, otherwise fall back to regular rendering
    if (useLODSystem) {
        renderCellsLOD(resolution, camera);
        return;
    }
    
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
	    {
		    TimerGPU timer("Instance extraction");

	    	extractShader->use();

	    	// Bind current buffers for compute shader (read from current cell buffer, write to current instance buffer)
	    	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
	    	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
	    	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, instanceBuffer); // Dispatch extract compute shader
	    	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer); // Bind GPU cell count buffer
	    	GLuint numGroups = (cellCount + 63) / 64;                                  // 64 threads per group
	    	extractShader->dispatch(numGroups, 1, 1);
	    	
	    	// Add barrier for instance extraction but don't flush yet
	    	addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	    }

        TimerGPU timer("Cell Rendering");
        
        // Flush barriers before rendering to ensure instance data is ready
        flushBarriers();

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
            glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndMass);
            float selectedRadius = selectedCell.cellData.getRadius();
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

    // Pass dragged cell index to skip its physics
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    physicsShader->setInt("u_draggedCellIndex", draggedIndex);

    // Set spatial grid uniforms
    physicsShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    physicsShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    physicsShader->setFloat("u_worldSize", config::WORLD_SIZE);
    physicsShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID); // Bind buffers (read from previous buffer, write to current buffer for stable simulation)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer()); // Read from previous frame
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridCountBuffer);

    // Also bind current buffer as output for physics results
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, getCellWriteBuffer()); // Write to current frame
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, gpuCellCountBuffer); // Bind GPU cell count buffer

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
    updateShader->setFloat("u_damping", 0.98f);

    // Pass dragged cell index to skip its position updates
    int draggedIndex = (isDraggingCell && selectedCell.isValid) ? selectedCell.cellIndex : -1;
    updateShader->setInt("u_draggedCellIndex", draggedIndex); // Bind current cell buffer for in-place updates
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellWriteBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gpuCellCountBuffer); // Bind GPU cell count buffer

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    updateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runInternalUpdateCompute(float deltaTime)
{
    TimerGPU timer("Cell Internal Update Compute");

    internalUpdateShader->use();

    // Set uniforms
    internalUpdateShader->setFloat("u_deltaTime", deltaTime);
    internalUpdateShader->setInt("u_maxCells", config::MAX_CELLS);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, modeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCellWriteBuffer()); // Read from current buffer (has physics results)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, cellAdditionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, idCounterBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, idPoolBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64; // Round up division
    internalUpdateShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::applyCellAdditions()
{
    TimerGPU timer("Cell Additions");

    cellAdditionShader->use();

    // Set uniforms
    cellAdditionShader->setInt("u_maxCells", cellLimit);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellAdditionBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, getCellWriteBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);

    // Dispatch compute shader
    GLuint numGroups = (cellLimit / 2 + 63) / 64; // Horrific over-dispatch but it's better than under-dispatch and surprisingly doesn't hurt performance
    cellAdditionShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Use optimized barrier for cell additions
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();

    GLuint zero = 0;
    glNamedBufferSubData(gpuCellCountBuffer, sizeof(GLuint), sizeof(GLuint), &zero); // offset = 4

    glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint) * 2);
}

void CellManager::resetSimulation()
{
    // Clear CPU-side data
    cpuCells.clear();
    cellStagingBuffer.clear();
    cellCount = 0;
    cpuPendingCellCount = 0;
    gpuPendingCellCount = 0;
    
    // CRITICAL FIX: Reset buffer rotation state for consistent keyframe restoration
    bufferRotation = 0;
    
    // Clear selection state
    clearSelection();
    
    // Clear GPU buffers by setting them to zero
    GLuint zero = 0;
    
    // Reset cell count buffers
    glNamedBufferSubData(gpuCellCountBuffer, 0, sizeof(GLuint), &zero); // cellCount = 0
    glNamedBufferSubData(gpuCellCountBuffer, sizeof(GLuint), sizeof(GLuint), &zero); // pendingCellCount = 0
    
    // Clear all cell buffers
    for (int i = 0; i < 3; i++)
    {
        glClearNamedBufferData(cellBuffer[i], GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }

    glClearNamedBufferData(instanceBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    
    // Clear addition buffer
    glClearNamedBufferData(cellAdditionBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    
    // Clear spatial grid buffers
    glClearNamedBufferData(gridBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferData(gridCountBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferData(gridOffsetBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    
    // Reset ID system
    struct IDCounters {
        uint32_t nextAvailableID = 1;      // Start from 1 (0 is reserved)
        uint32_t recycledIDCount = 0;      // No recycled IDs initially
        uint32_t maxCellID = 32767;        // Maximum cell ID (15 bits)
        uint32_t deadCellCount = 0;        // Dead cells found this frame
    } resetCounters;
    glNamedBufferSubData(idCounterBuffer, 0, sizeof(IDCounters), &resetCounters);
    glClearNamedBufferData(idPoolBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    glClearNamedBufferData(idRecycleBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    
    // Sync the staging buffer
    glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint) * 2);
    

}

void CellManager::spawnCells(int count)
{
    TimerCPU cpuTimer("Spawning Cells");

    for (int i = 0; i < count && cellCount < cellLimit; ++i)
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
        //float mass = 1.0f + static_cast<float>(rand()) / RAND_MAX * 2.0f;
        //float cellRadius = 1.0f; // All cells have the same radius of 1

        ComputeCell newCell{};
        newCell.positionAndMass = glm::vec4(position, 1.);
        newCell.velocity = glm::vec4(velocity, 0.);
        newCell.acceleration = glm::vec4(0.0f); // Reset acceleration
        
        // Assign unique ID (for spawned cells, parent ID = 0, cell ID = sequential, child flag = 0)
        // This is a simple approach for initial spawning - GPU will handle more complex ID management
        static uint16_t nextSpawnID = 1;
        newCell.setUniqueID(0, nextSpawnID++, 0);
        
        // Wrap around if we exceed the maximum ID
        if (nextSpawnID > 32767) {
            nextSpawnID = 1;
        }

        addCellToStagingBuffer(newCell);
    }
}

// Spatial partitioning
void CellManager::initializeSpatialGrid()
{
    // Create double buffered grid buffers to store cell indices

    glCreateBuffers(1, &gridBuffer);
    glNamedBufferData(gridBuffer,
                      config::TOTAL_GRID_CELLS * config::MAX_CELLS_PER_GRID * sizeof(GLuint),
                      nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create double buffered grid count buffers to store number of cells per grid cell
    glCreateBuffers(1, &gridCountBuffer);
    glNamedBufferData(gridCountBuffer,
                      config::TOTAL_GRID_CELLS * sizeof(GLuint),
                      nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create double buffered grid offset buffers for prefix sum calculations
    glCreateBuffers(1, &gridOffsetBuffer);
    glNamedBufferData(gridOffsetBuffer,
                      config::TOTAL_GRID_CELLS * sizeof(GLuint),
                      nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

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

    // HIGHLY OPTIMIZED: Minimal barriers for spatial grid operations
    // Step 1: Clear grid counts
    runGridClear();
    
    // Step 2: Count cells per grid cell (can run in parallel with clear on some GPUs)
    runGridAssign();
    
    // Single barrier after clear and assign - these operations write to different buffers
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();

    // Step 3: Calculate prefix sum for offsets (depends on assign results)
    runGridPrefixSum();

    // Step 4: Insert cells into grid (depends on prefix sum results)
    runGridInsert();
    
    // Add final barrier but don't flush - let caller decide when to flush
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::cleanupSpatialGrid()
{
    // Clean up double buffered spatial grid buffers

    if (gridBuffer != 0)
    {
        glDeleteBuffers(1, &gridBuffer);
        gridBuffer = 0;
    }
    if (gridCountBuffer != 0)
    {
        glDeleteBuffers(1, &gridCountBuffer);
        gridCountBuffer = 0;
    }
    if (gridOffsetBuffer != 0)
    {
        glDeleteBuffers(1, &gridOffsetBuffer);
        gridOffsetBuffer = 0;
    }
}

void CellManager::runGridClear()
{
    gridClearShader->use();

    gridClearShader->setInt("u_totalGridCells", config::TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridCountBuffer);

    GLuint numGroups = (config::TOTAL_GRID_CELLS + 63) / 64;
    gridClearShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridAssign()
{
    gridAssignShader->use();

    gridAssignShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    gridAssignShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    gridAssignShader->setFloat("u_worldSize", config::WORLD_SIZE);

    // Use previous buffer for spatial grid to match physics compute input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer); // Bind GPU cell count buffer

    GLuint numGroups = (cellCount + 63) / 64;
    gridAssignShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridPrefixSum()
{
    gridPrefixSumShader->use();

    gridPrefixSumShader->setInt("u_totalGridCells", config::TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridOffsetBuffer);

    GLuint numGroups = (config::TOTAL_GRID_CELLS + 63) / 64;
    gridPrefixSumShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::runGridInsert()
{
    gridInsertShader->use();    gridInsertShader->setInt("u_gridResolution", config::GRID_RESOLUTION);
    gridInsertShader->setFloat("u_gridCellSize", config::GRID_CELL_SIZE);
    gridInsertShader->setFloat("u_worldSize", config::WORLD_SIZE);
    gridInsertShader->setInt("u_maxCellsPerGrid", config::MAX_CELLS_PER_GRID); // Use previous buffer for spatial grid to match physics compute input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridOffsetBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, gpuCellCountBuffer); // Bind GPU cell count buffer

    GLuint numGroups = (cellCount + 63) / 64;
    gridInsertShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// Gizmo implementation

void CellManager::initializeGizmoBuffers()
{
    // Create buffer for gizmo line vertices (each cell produces 6 vertices for 3 lines)
    // Each vertex now has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &gizmoBuffer);
    glNamedBufferData(gizmoBuffer,
        cellLimit * 6 * sizeof(glm::vec4) * 2, // position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for gizmo rendering
    glCreateVertexArrays(1, &gizmoVAO);
    
    // Create VBO that will be bound to the gizmo buffer
    glCreateBuffers(1, &gizmoVBO);
    glNamedBufferData(gizmoVBO,
        cellLimit * 6 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(gizmoVAO, 0, gizmoVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(gizmoVAO, 0);
    glVertexArrayAttribFormat(gizmoVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(gizmoVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(gizmoVAO, 1);
    glVertexArrayAttribFormat(gizmoVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(gizmoVAO, 1, 0);
}

void CellManager::updateGizmoData()
{
    if (cellCount == 0) return;
    
    TimerGPU timer("Gizmo Data Update");
    
    gizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    gizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(gizmoBuffer, gizmoVBO, 0, 0, cellCount * 6 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos)
{
    if (!showGizmos || cellCount == 0) return;
    
    // Update gizmo data from current cell orientations
    updateGizmoData();
    
    TimerGPU timer("Gizmo Rendering");
    
    gizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    gizmoShader->setMat4("uProjection", projection);
    gizmoShader->setMat4("uView", view);
    
    // Enable depth testing and depth writing for proper depth sorting with ring gizmos
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // Enable line width for better visibility
    glLineWidth(4.0f);
    
    // Render gizmo lines
    glBindVertexArray(gizmoVAO);
    glDrawArrays(GL_LINES, 0, cellCount * 6); // 6 vertices per cell (3 lines * 2 vertices)
    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void CellManager::cleanupGizmos()
{
    if (gizmoBuffer != 0)
    {
        glDeleteBuffers(1, &gizmoBuffer);
        gizmoBuffer = 0;
    }
    if (gizmoVBO != 0)
    {
        glDeleteBuffers(1, &gizmoVBO);
        gizmoVBO = 0;
    }
    if (gizmoVAO != 0)
    {
        glDeleteVertexArrays(1, &gizmoVAO);
        gizmoVAO = 0;
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
        glm::vec3 cellPosition = glm::vec3(cpuCells[i].positionAndMass);
        float cellRadius = cpuCells[i].getRadius();

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
    selectedCell.cellIndex = -1;
    selectedCell.isValid = false;
    isDraggingCell = false;
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
    if (cellCount == 0)
        return;

    // OPTIMIZED: Use barrier batching for GPU sync
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from GPU buffer to staging buffer (no GPU->CPU transfer warning)
    glCopyNamedBufferSubData(getCellReadBuffer(), stagingCellBuffer, 0, 0, cellCount * sizeof(ComputeCell));
    
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
        cpuCells.reserve(cellCount);
        // Copy data from staging buffer to CPU storage
        for (int i = 0; i < cellCount; i++)
        {
            if (i < cpuCells.size())
            {
                cpuCells[i] = stagedData[i]; // Sync all data
            } else
            {
                cpuCells.push_back(stagedData[i]);
            }
        }

        std::cout << "Synced " << cellCount << " cell positions from GPU via staging buffer" << std::endl;
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

// Ring Gizmo Implementation

void CellManager::initializeRingGizmoBuffers()
{
    // Create buffer for ring vertices (each cell produces 2 rings * 32 segments * 6 vertices = 384 vertices)
    // Each vertex has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &ringGizmoBuffer);
    glNamedBufferData(ringGizmoBuffer,
        cellLimit * 384 * sizeof(glm::vec4) * 2, // position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for ring gizmo rendering
    glCreateVertexArrays(1, &ringGizmoVAO);
    
    // Create VBO that will be bound to the ring gizmo buffer
    glCreateBuffers(1, &ringGizmoVBO);
    glNamedBufferData(ringGizmoVBO,
        cellLimit * 384 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(ringGizmoVAO, 0, ringGizmoVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(ringGizmoVAO, 0);
    glVertexArrayAttribFormat(ringGizmoVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(ringGizmoVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(ringGizmoVAO, 1);
    glVertexArrayAttribFormat(ringGizmoVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(ringGizmoVAO, 1, 0);
}

void CellManager::updateRingGizmoData()
{
    if (cellCount == 0) return;
    
    TimerGPU timer("Ring Gizmo Data Update");
    
    ringGizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind mode data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    // Bind ring gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ringGizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    ringGizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(ringGizmoBuffer, ringGizmoVBO, 0, 0, cellCount * 384 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderRingGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager)
{
    if (!uiManager.showOrientationGizmos || cellCount == 0) return;
    
    // Update ring gizmo data from current cell orientations and split directions
    updateRingGizmoData();
    
    TimerGPU timer("Ring Gizmo Rendering");
    
    ringGizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    ringGizmoShader->setMat4("uProjection", projection);
    ringGizmoShader->setMat4("uView", view);
    
    // Enable face culling so rings are only visible from one side
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable depth testing but disable depth writing to avoid z-fighting with spheres
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // Enable blending for better visibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render ring gizmo triangles (each cell has 2 rings, each ring has 32 segments * 6 vertices)
    glBindVertexArray(ringGizmoVAO);
    
    // Render each ring as triangles - both rings with proper depth testing
    // Blue ring will be visible from one side, red ring from the other side
    for (int i = 0; i < cellCount; i++) {
        // Blue ring (positioned forward along split direction)
        glDrawArrays(GL_TRIANGLES, i * 384, 192);
        // Red ring (positioned backward along split direction)
        glDrawArrays(GL_TRIANGLES, i * 384 + 192, 192);
    }
    
    glBindVertexArray(0);
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

void CellManager::cleanupRingGizmos()
{
    if (ringGizmoBuffer != 0)
    {
        glDeleteBuffers(1, &ringGizmoBuffer);
        ringGizmoBuffer = 0;
    }
    if (ringGizmoVBO != 0)
    {
        glDeleteBuffers(1, &ringGizmoVBO);
        ringGizmoVBO = 0;
    }
    if (ringGizmoVAO != 0)
    {
        glDeleteVertexArrays(1, &ringGizmoVAO);
        ringGizmoVAO = 0;
    }
}

// ID Management System Implementation

void CellManager::initializeIDSystem()
{
    // Create ID counter buffer
    glCreateBuffers(1, &idCounterBuffer);
    
    // Initialize counter data
    struct IDCounters {
        uint32_t nextAvailableID = 1;      // Start from 1 (0 is reserved)
        uint32_t recycledIDCount = 0;      // No recycled IDs initially
        uint32_t maxCellID = 32767;        // Maximum cell ID (15 bits)
        uint32_t deadCellCount = 0;        // Dead cells found this frame
    } initialCounters;
    
    glNamedBufferData(idCounterBuffer, sizeof(IDCounters), &initialCounters, GL_DYNAMIC_COPY);
    
    // Create ID pool buffer for available/recycled IDs
    glCreateBuffers(1, &idPoolBuffer);
    glNamedBufferData(idPoolBuffer, cellLimit * sizeof(uint32_t), nullptr, GL_DYNAMIC_COPY);
    
    // Create ID recycle buffer for dead cell IDs
    glCreateBuffers(1, &idRecycleBuffer);
    glNamedBufferData(idRecycleBuffer, cellLimit * sizeof(uint32_t), nullptr, GL_DYNAMIC_COPY);
}

void CellManager::cleanupIDSystem()
{
    if (idCounterBuffer != 0)
    {
        glDeleteBuffers(1, &idCounterBuffer);
        idCounterBuffer = 0;
    }
    if (idPoolBuffer != 0)
    {
        glDeleteBuffers(1, &idPoolBuffer);
        idPoolBuffer = 0;
    }
    if (idRecycleBuffer != 0)
    {
        glDeleteBuffers(1, &idRecycleBuffer);
        idRecycleBuffer = 0;
    }
}

void CellManager::runIDManager()
{
    if (cellCount == 0) return;
    
    idManagerShader->use();
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, idCounterBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, idPoolBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, idRecycleBuffer);
    
    // Set uniforms
    idManagerShader->setInt("u_maxCells", cellLimit);
    idManagerShader->setFloat("u_minMass", 0.01f); // Minimum mass to consider a cell alive
    
    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    idManagerShader->dispatch(numGroups, 1, 1);
    
    // Add barrier but don't flush - let caller handle it
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::recycleDeadCellIDs()
{
    // This is now handled by the runIDManager() function
    // Called automatically during the update cycle
}

void CellManager::printCellIDs(int maxCells)
{
    if (cellCount == 0) {
        std::cout << "No cells to display IDs for.\n";
        return;
    }
    
    // Sync cell data from GPU for debugging
    syncCellPositionsFromGPU();
    
    std::cout << "Cell IDs (showing first " << std::min(maxCells, cellCount) << " cells):\n";
    for (int i = 0; i < std::min(maxCells, cellCount); i++) {
        if (i < static_cast<int>(cellStagingBuffer.size())) {
            const ComputeCell& cell = cellStagingBuffer[i];
            uint16_t parentID = cell.getParentID();
            uint16_t cellID = cell.getCellID();
            uint8_t childFlag = cell.getChildFlag();
            char childChar = (childFlag == 0) ? 'A' : 'B';
            
            std::cout << "Cell " << i << ": " << parentID << "." << cellID << "." << childChar 
                      << " (raw: 0x" << std::hex << cell.uniqueID << std::dec << ")\n";
        }
    }
}

// Adhesion Line Implementation

void CellManager::initializeAdhesionLineBuffers()
{
    // Create buffer for adhesion line vertices (each line has 2 vertices)
    // Each vertex has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &adhesionLineBuffer);
    glNamedBufferData(adhesionLineBuffer,
        cellLimit * 2 * sizeof(glm::vec4) * 2, // 2 vertices per line, position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for adhesion line rendering
    glCreateVertexArrays(1, &adhesionLineVAO);
    
    // Create VBO that will be bound to the adhesion line buffer
    glCreateBuffers(1, &adhesionLineVBO);
    glNamedBufferData(adhesionLineVBO,
        cellLimit * 2 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(adhesionLineVAO, 0, adhesionLineVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(adhesionLineVAO, 0);
    glVertexArrayAttribFormat(adhesionLineVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(adhesionLineVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(adhesionLineVAO, 1);
    glVertexArrayAttribFormat(adhesionLineVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(adhesionLineVAO, 1, 0);
}

void CellManager::updateAdhesionLineData()
{
    if (cellCount == 0) return;
    
    TimerGPU timer("Adhesion Line Data Update");
    
    adhesionLineExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    // Bind mode data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    // Bind adhesion line buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adhesionLineBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    adhesionLineExtractShader->dispatch(numGroups, 1, 1);
    
    // Use targeted barrier for buffer copy
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    flushBarriers();
    
    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(adhesionLineBuffer, adhesionLineVBO, 0, 0, cellCount * 2 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CellManager::renderAdhesionLines(glm::vec2 resolution, const Camera& camera, bool showAdhesionLines)
{
    if (!showAdhesionLines || cellCount == 0) return;
    
    // Update adhesion line data from current cell positions and IDs
    updateAdhesionLineData();
    
    TimerGPU timer("Adhesion Line Rendering");
    
    adhesionLineShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    adhesionLineShader->setMat4("uProjection", projection);
    adhesionLineShader->setMat4("uView", view);
    
    // Enable depth testing for proper 3D rendering
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // Enable line width for better visibility
    glLineWidth(3.0f);
    
    // Render adhesion lines
    glBindVertexArray(adhesionLineVAO);
    glDrawArrays(GL_LINES, 0, cellCount * 2); // 2 vertices per line, one line per cell pair
    glBindVertexArray(0);
    
    // Reset line width
    glLineWidth(1.0f);
}

void CellManager::cleanupAdhesionLines()
{
    if (adhesionLineBuffer != 0)
    {
        glDeleteBuffers(1, &adhesionLineBuffer);
        adhesionLineBuffer = 0;
    }
    if (adhesionLineVBO != 0)
    {
        glDeleteBuffers(1, &adhesionLineVBO);
        adhesionLineVBO = 0;
    }
    if (adhesionLineVAO != 0)
    {
        glDeleteVertexArrays(1, &adhesionLineVAO);
        adhesionLineVAO = 0;
    }
}

// LOD System Implementation

void CellManager::initializeLODSystem()
{
    // Initialize LOD shaders
    lodComputeShader = new Shader("shaders/sphere_lod.comp");
    lodVertexShader = new Shader("shaders/sphere_lod.vert", "shaders/sphere_lod.frag");
    
    // Generate LOD sphere meshes
    sphereMesh.generateLODSpheres(1.0f);
    sphereMesh.setupLODBuffers();
    sphereMesh.setupLODInstanceBuffer(instanceBuffer); // Use existing instance buffer
    
    std::cout << "LOD system initialized with " << SphereMesh::LOD_LEVELS << " detail levels\n";
}

void CellManager::cleanupLODSystem()
{
    delete lodComputeShader;
    delete lodVertexShader;
    lodComputeShader = nullptr;
    lodVertexShader = nullptr;
}

void CellManager::runLODCompute(const Camera& camera)
{
    if (cellCount == 0) return;
    
    TimerGPU timer("LOD Instance Extraction");
    
    lodComputeShader->use();
    
    // Set uniforms
    lodComputeShader->setVec3("u_cameraPos", camera.getPosition());
    lodComputeShader->setFloat("u_lodDistances[0]", lodDistances[0]);
    lodComputeShader->setFloat("u_lodDistances[1]", lodDistances[1]);
    lodComputeShader->setFloat("u_lodDistances[2]", lodDistances[2]);
    lodComputeShader->setFloat("u_lodDistances[3]", lodDistances[3]);
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, getCellReadBuffer());
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, modeBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, instanceBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (cellCount + 63) / 64;
    lodComputeShader->dispatch(numGroups, 1, 1);
    
    addBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void CellManager::updateLODLevels(const Camera& camera)
{
    if (!useLODSystem || cellCount == 0) return;
    
    // Run LOD compute to generate instance data with LOD information
    runLODCompute(camera);
    
    flushBarriers();
}

void CellManager::renderCellsLOD(glm::vec2 resolution, const Camera& camera)
{
    if (cellCount == 0 || !useLODSystem) {
        return;
    }

    // Safety checks
    if (resolution.x <= 0 || resolution.y <= 0 || resolution.x < 1 || resolution.y < 1) {
        return;
    }

    try {
        // Update instance data with LOD information
        updateLODLevels(camera);
        
        TimerGPU timer("LOD Cell Rendering");
        
        // Use LOD shaders
        lodVertexShader->use();
        
        // Set up camera matrices
        glm::mat4 view = camera.getViewMatrix();
        float aspectRatio = resolution.x / resolution.y;
        if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
            aspectRatio = 16.0f / 9.0f;
        }
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
        
        // Set uniforms
        lodVertexShader->setMat4("uProjection", projection);
        lodVertexShader->setMat4("uView", view);
        lodVertexShader->setVec3("uCameraPos", camera.getPosition());
        lodVertexShader->setVec3("uLightDir", glm::vec3(1.0f, 1.0f, 1.0f));
        
        // Selection highlighting uniforms
        if (selectedCell.isValid) {
            glm::vec3 selectedPos = glm::vec3(selectedCell.cellData.positionAndMass);
            float selectedRadius = selectedCell.cellData.getRadius();
            lodVertexShader->setVec3("uSelectedCellPos", selectedPos);
            lodVertexShader->setFloat("uSelectedCellRadius", selectedRadius);
        } else {
            lodVertexShader->setVec3("uSelectedCellPos", glm::vec3(-9999.0f));
            lodVertexShader->setFloat("uSelectedCellRadius", 0.0f);
        }
        lodVertexShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
        
        // Enable depth testing
        glEnable(GL_DEPTH_TEST);
        
        // For the consolidated LOD system, we render all cells with different mesh detail levels
        // based on their average distance from camera. This is a simplified approach that
        // provides good performance without complex GPU sorting.
        
        glm::vec3 cameraPos = camera.getPosition();
        
        // Calculate average distance to determine which LOD level to use for most cells
        float avgDistance = glm::length(cameraPos) / std::max(1.0f, std::sqrt(static_cast<float>(cellCount)));
        
        int primaryLOD = 3; // Default to lowest detail
        if (avgDistance < lodDistances[0]) {
            primaryLOD = 0; // Highest detail
        } else if (avgDistance < lodDistances[1]) {
            primaryLOD = 1; // High detail
        } else if (avgDistance < lodDistances[2]) {
            primaryLOD = 2; // Medium detail
        }
        
        // Render all cells using the primary LOD level
        sphereMesh.renderLOD(primaryLOD, cellCount, 0);
        
        // For nearby cells, also render with higher detail (additive approach)
        if (primaryLOD > 0 && avgDistance < lodDistances[primaryLOD-1] * 2.0f) {
            // Render subset with higher detail for close objects
            int nearbyCount = std::min(cellCount / 4, 1000); // Limit for performance
            sphereMesh.renderLOD(primaryLOD - 1, nearbyCount, 0);
        }
        
    } catch (const std::exception &e) {
        std::cerr << "Exception in renderCellsLOD: " << e.what() << "\n";
        // Fall back to regular rendering
        useLODSystem = false;
    } catch (...) {
        std::cerr << "Unknown exception in renderCellsLOD\n";
        useLODSystem = false;
    }
}
