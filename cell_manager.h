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
    glm::vec4 positionAndMass{0, 0, 0, 1};       // x, y, z, mass
    glm::vec4 velocity{};
    glm::vec4 acceleration{};
    glm::quat orientation{1., 0., 0., 0.};  // angular stuff in quaternions to prevent gimbal lock
    glm::quat angularVelocity{ 1., 0., 0., 0. };
    glm::quat angularAcceleration{ 1., 0., 0., 0. };

    // Internal:
    glm::vec4 signallingSubstances{}; // 4 substances for now
    int modeIndex{ 0 };
    float age{ 0 };                      // also used for split timer
    float toxins{ 0 };
    float nitrates{ 1 };    float getRadius() const
    {
        return static_cast<float>(pow(positionAndMass.w, 1.0f/3.0f));
    }
};

struct CellManager
{
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates

    // GPU buffer objects - Double buffered for performance
    GLuint cellBuffer[3]{};         // SSBO for compute cell data (double buffered)
    GLuint instanceBuffer{};        // VBO for instance rendering data
    int bufferRotation{};

    // Cell count management
    GLuint gpuCellCountBuffer{};     // GPU-accessible cell count buffer
    GLuint stagingCellCountBuffer{}; // CPU-accessible cell count buffer (no sync stalls)
    GLuint cellAdditionBuffer{};     // Cell addition queue for GPU

    // Genome buffer (immutable, no need for double buffering)
    // It might be a good idea in the future to switch from a flattened mode array to genome structs that contain their own mode arrays
    GLuint modeBuffer{};

    // Spatial partitioning buffers - Double buffered
    GLuint gridBuffer{};       // SSBO for grid cell data (stores cell indices)
    GLuint gridCountBuffer{};  // SSBO for grid cell counts
    GLuint gridOffsetBuffer{}; // SSBO for grid cell starting offsets

    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // Asynchronous readback system for performance monitoring
    //GLuint readbackBuffer = 0;      // Buffer for async GPU->CPU data transfer
    //GLsync readbackFence = nullptr; // Sync object for async operations
    //bool readbackInProgress = false;
    //float readbackCooldown = 0.0f; // Timer to limit readback frequency

    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    Shader* extractShader = nullptr; // For extracting instance data efficiently
    Shader* internalUpdateShader = nullptr;
    Shader* cellCounterShader = nullptr;
	Shader* cellAdditionShader = nullptr;

    // Spatial partitioning compute shaders
    Shader* gridClearShader = nullptr;     // Clear grid counts
    Shader* gridAssignShader = nullptr;    // Assign cells to grid
    Shader* gridPrefixSumShader = nullptr; // Calculate grid offsets
    Shader* gridInsertShader = nullptr;    // Insert cells into grid
    
    // CPU-side storage for initialization and debugging
    // Note: cpuCells is deprecated in favor of GPU buffers, should be removed after refactoring
    std::vector<ComputeCell> cpuCells;
    std::vector<ComputeCell> cellStagingBuffer;
    
    // Cell count tracking (CPU-side approximation of GPU state)
    int cellCount{0};               // Approximate cell count, may not reflect exact GPU state due to being a frame behind
    int cpuPendingCellCount{0};     // Number of cells pending addition by CPU
    int gpuPendingCellCount{0};     // Approx number of cells pending addition by GPU
	// Mysteriously the value read on cpu is always undershooting significantly so you're better off treating it as a bool than an int
    void* mappedPtr = nullptr;      // Pointer to the cell count staging buffer
    GLuint* countPtr = nullptr;     // Typed pointer to the mapped buffer value

    // Configuration
    static constexpr int MAX_CELLS = config::MAX_CELLS;
    static constexpr int DEFAULT_CELL_COUNT = config::DEFAULT_CELL_COUNT;
    float spawnRadius = config::DEFAULT_SPAWN_RADIUS;
    int cellLimit = config::MAX_CELLS;

    // Constructor and destructor
    CellManager();
    ~CellManager();

    // We declare functions in the struct, but we will define them in the cell_manager.cpp file.
    // This is because when a file is edited, the compiler will also have to recompile all the files that include it.
    // So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.

    void initializeGPUBuffers();
    void resetSimulation();
    void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader &cellShader, class Camera &camera);
    void initializeGizmoBuffers();
    void updateGizmoData();
    void cleanupGizmos();
    
    // Ring gizmo methods
    void renderRingGizmos(glm::vec2 resolution, const class Camera &camera, const class UIManager &uiManager);
    void initializeRingGizmoBuffers();
    void cleanupRingGizmos();

    void addCellsToGPUBuffer(const std::vector<ComputeCell> &cells);
    void addCellToGPUBuffer(const ComputeCell &newCell);
    void addCellToStagingBuffer(const ComputeCell &newCell);
    void addCell(const ComputeCell &newCell) { addCellToStagingBuffer(newCell); }
    void addStagedCellsToGPUBuffer();
    void addGenomeToBuffer(GenomeData& genomeData) const;
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
    //bool isReadbackInProgress() const { return readbackInProgress; }
    //bool isReadbackSystemHealthy() const { return readbackBuffer != 0; }
    //float getReadbackCooldown() const { return readbackCooldown; }

    // Performance testing function
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

    // Asynchronous readback functions for performance monitoring // not actually implemented yet, maybe later if we need to
    //void initializeReadbackSystem();
    //void updateReadbackSystem(float deltaTime);
    //void requestAsyncReadback();
    //bool checkAsyncReadback(ComputeCell *outputData, int maxCells);
    //void cleanupReadbackSystem();

    // Double buffering management functions
    int getRotatedIndex(int index, int max) const { return (index + bufferRotation) % max; }
    void rotateBuffers() { bufferRotation = getRotatedIndex(1, 3); }
    GLuint getCellReadBuffer() const { return cellBuffer[getRotatedIndex(0, 3)]; }
    GLuint getCellWriteBuffer() const { return cellBuffer[getRotatedIndex(1, 3)]; }

    void setCellLimit(int limit) { cellLimit = limit; }
    int getCellLimit() const { return cellLimit; }

private:
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
    void runInternalUpdateCompute(float deltaTime);
    void runCellCounter();
    void applyCellAdditions();

    // Spatial grid helper functions
    void runGridClear();
    void runGridAssign();
    void runGridPrefixSum();
    void runGridInsert();
};
