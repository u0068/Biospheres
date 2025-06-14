#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

#include "shader_class.h"
#include "input.h"
#include "config.h"
#include "sphere_mesh.h"
#include "config.h"

// Forward declaration
class Camera;

// Structure of Arrays (SoA) approach for better GPU performance
// Instead of Array of Structures (AoS), we separate each property into its own array
struct CellDataSoA
{
    // Separate arrays for each property - much more cache-friendly for GPU compute
    std::vector<glm::vec3> positions;     // x, y, z coordinates
    std::vector<float> radii;             // cell radii
    std::vector<glm::vec3> velocities;    // velocity vectors
    std::vector<float> masses;            // cell masses
    std::vector<glm::vec3> accelerations; // acceleration vectors

    // Helper functions to maintain consistency
    void resize(size_t count)
    {
        positions.resize(count);
        radii.resize(count);
        velocities.resize(count);
        masses.resize(count);
        accelerations.resize(count);
    }

    void reserve(size_t count)
    {
        positions.reserve(count);
        radii.reserve(count);
        velocities.reserve(count);
        masses.reserve(count);
        accelerations.reserve(count);
    }

    size_t size() const
    {
        return positions.size();
    }

    void clear()
    {
        positions.clear();
        radii.clear();
        velocities.clear();
        masses.clear();
        accelerations.clear();
    }
};

// Legacy AoS structure for backward compatibility during transition
struct ComputeCell
{
    glm::vec4 positionAndRadius; // x, y, z, radius
    glm::vec4 velocityAndMass;   // vx, vy, vz, mass
    glm::vec4 acceleration;      // ax, ay, az, unused
};

struct CellManager
{
    // GPU-based cell management using compute shaders with SoA approach
    // Structure of Arrays provides better memory access patterns for GPU compute

    // GPU buffer objects for SoA layout
    GLuint positionBuffer = 0;     // SSBO for vec3 positions
    GLuint radiusBuffer = 0;       // SSBO for float radii
    GLuint velocityBuffer = 0;     // SSBO for vec3 velocities
    GLuint massBuffer = 0;         // SSBO for float masses
    GLuint accelerationBuffer = 0; // SSBO for vec3 accelerations
    GLuint instanceBuffer = 0;     // VBO for instance rendering data (position + radius)

    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // Asynchronous readback system for performance monitoring
    GLuint readbackBuffer = 0;      // Buffer for async GPU->CPU data transfer
    GLsync readbackFence = nullptr; // Sync object for async operations
    bool readbackInProgress = false;
    float readbackCooldown = 0.0f; // Timer to limit readback frequency

    // Compute shaders (updated for SoA)
    Shader *physicsShader = nullptr;
    Shader *updateShader = nullptr;
    Shader *extractShader = nullptr; // For extracting instance data efficiently

    // CPU-side storage using SoA layout
    CellDataSoA cellData;                       // Main SoA data structure
    std::vector<ComputeCell> cellStagingBuffer; // Temporary buffer for legacy compatibility
    int cellCount{0};                           // Current number of active cells
    int pendingCellCount{0};                    // Number of cells pending addition

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
    // SoA-based functions
    void initializeGPUBuffers();
    void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader &cellShader, class Camera &camera);
    void addCellsToGPU(const CellDataSoA &newCells, int count);
    void addCellToStagingBuffer(const ComputeCell &newCell); // Legacy compatibility
    void addCell(const ComputeCell &newCell) { addCellToStagingBuffer(newCell); }
    void addStagedCellsToGPU();
    void updateCells(float deltaTime);
    void cleanup();

    // Helper functions for SoA conversion
    void convertAoSToSoA(const std::vector<ComputeCell> &aosData, CellDataSoA &soaData);
    ComputeCell getAoSCell(int index) const; // Get cell in legacy format for compatibility

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
                               const glm::vec3 &sphereCenter, float sphereRadius, float &distance); // Getters for selection system (updated for SoA)
    bool hasSelectedCell() const { return selectedCell.isValid; }
    const SelectedCellInfo &getSelectedCell() const { return selectedCell; }
    ComputeCell getCellData(int index) const { return getAoSCell(index); } // Legacy compatibility
    void updateCellData(int index, const ComputeCell &newData);            // Legacy compatibility - converts to SoA

    // Asynchronous readback functions for performance monitoring
    void initializeReadbackSystem();
    void updateReadbackSystem(float deltaTime);
    void requestAsyncReadback();
    bool checkAsyncReadback(ComputeCell *outputData, int maxCells);
    void cleanupReadbackSystem();

private:
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
};
