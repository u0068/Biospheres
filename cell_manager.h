#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

#include "shader_class.h"
#include "input.h"
#include "config.h"
#include "sphere_mesh.h"
#include "config.h"
#include "genome.h"

// Forward declaration
class Camera;

// GPU compute cell structure matching the compute shader
struct ComputeCell {
    // Physics:
    glm::vec4 positionAndMass{ glm::vec4(0, 0, 0, 1) };       // x, y, z, mass
    glm::vec3 velocity{};
    glm::vec3 acceleration{};
    glm::vec4 orientation{};          // angular stuff in quaternion to prevent gimbal lock
    glm::vec4 angularVelocity{};
    glm::vec4 angularAcceleration{};

    // Internal:
    glm::vec4 signallingSubstances{}; // 4 substances for now
    int modeIndex{ 0 };
    float age{ 0 };                      // also used for split timer
    float toxins{ 0 };
    float nitrates{ 1 };

    float getRadius() const
    {
        return pow(positionAndMass.w, 1 / 3);
    }
};

struct CellManager
{
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates    // GPU buffer objects - Double buffered for performance
    GLuint cellBuffer[2] = {0, 0};     // SSBO for compute cell data (double buffered)
    GLuint instanceBuffer[2] = {0, 0}; // VBO for instance rendering data (double buffered)
    int currentBufferIndex = 0;        // Index of current buffer (0 or 1)
    int previousBufferIndex = 1;       // Index of previous buffer (1 or 0)

    // Genome buffer, no need for double buffering as, ironically, genomes are immutable
    GLuint genomeBuffer = 0;

    // Spatial partitioning buffers - Double buffered
    GLuint gridBuffer[2] = {0, 0};       // SSBO for grid cell data (stores cell indices)
    GLuint gridCountBuffer[2] = {0, 0};  // SSBO for grid cell counts
    GLuint gridOffsetBuffer[2] = {0, 0}; // SSBO for grid cell starting offsets

    // Buffer bindings, universal across all shaders for convenience
	// Odd is previous, Even is current
    // 0    : modes(single buffer)
    // 1, 2 : cellBuffer
    // 3, 4 : instanceBuffer
    // 5, 6 : gridBuffer
    // 7, 8 : gridCountBuffer
    // 9, 10 : gridOffsetBuffer
    // 11 onwards: any future buffers, like type ECS stuff

    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // Asynchronous readback system for performance monitoring
    GLuint readbackBuffer = 0;      // Buffer for async GPU->CPU data transfer
    GLsync readbackFence = nullptr; // Sync object for async operations
    bool readbackInProgress = false;
    float readbackCooldown = 0.0f; // Timer to limit readback frequency

    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    Shader* extractShader = nullptr; // For extracting instance data efficiently
    Shader* internalUpdateShader = nullptr; // For extracting instance data efficiently

    // Spatial partitioning compute shaders
    Shader* gridClearShader = nullptr;     // Clear grid counts
    Shader* gridAssignShader = nullptr;    // Assign cells to grid
    Shader* gridPrefixSumShader = nullptr; // Calculate grid offsets
    Shader* gridInsertShader = nullptr;    // Insert cells into grid

    // CPU-side storage for initialization and debugging
    std::vector<ComputeCell> cpuCells; // Deprecated, since we use GPU buffers now. Get rid of this after refactoring.
    std::vector<ComputeCell> cellStagingBuffer;
    int cellCount{0};        // Not sure if this is accurately representative of the GPU state, im gonna need to work on it
    int pendingCellCount{0}; // Number of cells pending addition

    // Configuration
    static constexpr int MAX_CELLS = config::MAX_CELLS;
    static constexpr int DEFAULT_CELL_COUNT = config::DEFAULT_CELL_COUNT;
    float spawnRadius = config::DEFAULT_SPAWN_RADIUS; // Acts as both spawn area and containment barrier

    // Constructor and destructor
    CellManager();
    ~CellManager();

    // We declare functions in the struct, but we will define them in the cell_manager.cpp file.
    // This is because when a file is edited, the compiler will also have to recompile all the files that include it.
    // So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.

    void initializeGPUBuffers();
    void resetSimulation();
    //void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader &cellShader, class Camera &camera);
    void addCellsToGPUBuffer(const std::vector<ComputeCell> &cells);
    void addCellToGPUBuffer(const ComputeCell &newCell);
    void addCellToStagingBuffer(const ComputeCell &newCell);
    void addCell(const ComputeCell &newCell) { addCellToStagingBuffer(newCell); }
    void addStagedCellsToGPUBuffer();
    void addGenomeToBuffer(GenomeData& genomeData);
    void updateCells(float deltaTime);
    void cleanup();

    // Spatial partitioning functions
    void initializeSpatialGrid();
    void updateSpatialGrid();
    void cleanupSpatialGrid();

    // Getter functions for debug information
    int getCellCount() const { return cellCount; }
    float getSpawnRadius() const { return spawnRadius; }

    // GPU pipeline status getters
    bool isReadbackInProgress() const { return readbackInProgress; }
    bool isReadbackSystemHealthy() const { return readbackBuffer != 0; }
    float getReadbackCooldown() const { return readbackCooldown; }

    // Performance testing function
    void setActiveCellCount(int count)
    {
        if (count <= cellCount && count >= 0)
        {
            // This allows reducing active cells for performance testing
            // without changing the actual cell count or buffer data
        }
    } // Cell selection and interaction system
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

    // Selection and interaction functions
    void handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                          const class Camera &camera, bool isMousePressed, bool isMouseDown,
                          float scrollDelta = 0.0f);
    int selectCellAtPosition(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection);
    void dragSelectedCell(const glm::vec3 &newWorldPosition);
    void clearSelection();

    // Handle the end of dragging (restore physics)
    void endDrag();

    // GPU synchronization for selection (synchronous readback for immediate use)
    void syncCellPositionsFromGPU();

    // Utility functions for mouse interaction
    glm::vec3 calculateMouseRay(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                                const class Camera &camera);
    bool raySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                               const glm::vec3 &sphereCenter, float sphereRadius, float &distance);

    // Getters for selection system
    bool hasSelectedCell() const { return selectedCell.isValid; }
    const SelectedCellInfo &getSelectedCell() const { return selectedCell; }
    ComputeCell getCellData(int index) const;
    void updateCellData(int index, const ComputeCell &newData); // Needs refactoring

    // Asynchronous readback functions for performance monitoring
    void initializeReadbackSystem();
    void updateReadbackSystem(float deltaTime);
    void requestAsyncReadback();
    bool checkAsyncReadback(ComputeCell *outputData, int maxCells);
    void cleanupReadbackSystem();

    // Double buffering management functions
    void swapBuffers();
    GLuint getCurrentCellBuffer() const { return cellBuffer[currentBufferIndex]; }
    GLuint getPreviousCellBuffer() const { return cellBuffer[previousBufferIndex]; }
    GLuint getCurrentInstanceBuffer() const { return instanceBuffer[currentBufferIndex]; }
    GLuint getPreviousInstanceBuffer() const { return instanceBuffer[previousBufferIndex]; }
    GLuint getCurrentGridBuffer() const { return gridBuffer[currentBufferIndex]; }
    GLuint getCurrentGridCountBuffer() const { return gridCountBuffer[currentBufferIndex]; }
    GLuint getCurrentGridOffsetBuffer() const { return gridOffsetBuffer[currentBufferIndex]; }

private:
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
    void runInternalUpdateCompute(float deltaTime);

    // Spatial grid helper functions
    void runGridClear();
    void runGridAssign();
    void runGridPrefixSum();
    void runGridInsert();
};
