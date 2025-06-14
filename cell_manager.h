#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>

#include "buffer_manager.h"
#include "shader_class.h"
#include "input.h"
#include "config.h"
#include "sphere_mesh.h"
#include "config.h"

// Forward declaration
class Camera;

// GPU compute cell structure matching the compute shader (AoS - for backward compatibility)
struct ComputeCell
{
    glm::vec4 positionAndRadius; // x, y, z, radius
    glm::vec4 velocityAndMass;   // vx, vy, vz, mass
    glm::vec4 acceleration;      // ax, ay, az, unused
};

// Structure of Arrays for efficient CPU-side cell data management
struct CellDataSoA
{
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> velocities;
    std::vector<glm::vec3> accelerations;
    std::vector<float> masses;
    std::vector<float> radii;

    size_t size() const { return positions.size(); }

    void reserve(size_t count)
    {
        positions.reserve(count);
        velocities.reserve(count);
        accelerations.reserve(count);
        masses.reserve(count);
        radii.reserve(count);
    }

    void resize(size_t count)
    {
        positions.resize(count);
        velocities.resize(count);
        accelerations.resize(count);
        masses.resize(count);
        radii.resize(count);
    }

    void clear()
    {
        positions.clear();
        velocities.clear();
        accelerations.clear();
        masses.clear();
        radii.clear();
    }

    void addCell(const glm::vec3 &pos, const glm::vec3 &vel, const glm::vec3 &accel, float mass, float radius)
    {
        positions.push_back(pos);
        velocities.push_back(vel);
        accelerations.push_back(accel);
        masses.push_back(mass);
        radii.push_back(radius);
    }

    // Convert from AoS ComputeCell (for compatibility)
    void addCell(const ComputeCell &cell)
    {
        positions.push_back(glm::vec3(cell.positionAndRadius));
        velocities.push_back(glm::vec3(cell.velocityAndMass));
        accelerations.push_back(glm::vec3(cell.acceleration));
        masses.push_back(cell.velocityAndMass.w);
        radii.push_back(cell.positionAndRadius.w);
    }

    // Convert to AoS ComputeCell at index (for compatibility)
    ComputeCell getCell(size_t index) const
    {
        if (index >= size())
            return {};
        ComputeCell cell;
        cell.positionAndRadius = glm::vec4(positions[index], radii[index]);
        cell.velocityAndMass = glm::vec4(velocities[index], masses[index]);
        cell.acceleration = glm::vec4(accelerations[index], 0.0f);
        return cell;
    }

    // Get data pointers for GPU upload (as vec4 arrays for compatibility with existing GPU buffers)
    std::vector<glm::vec4> getPositionAndRadiusData() const
    {
        std::vector<glm::vec4> data;
        data.reserve(size());
        for (size_t i = 0; i < size(); ++i)
        {
            data.emplace_back(positions[i], radii[i]);
        }
        return data;
    }

    std::vector<glm::vec4> getVelocityAndMassData() const
    {
        std::vector<glm::vec4> data;
        data.reserve(size());
        for (size_t i = 0; i < size(); ++i)
        {
            data.emplace_back(velocities[i], masses[i]);
        }
        return data;
    }

    std::vector<glm::vec4> getAccelerationData() const
    {
        std::vector<glm::vec4> data;
        data.reserve(size());
        for (size_t i = 0; i < size(); ++i)
        {
            data.emplace_back(accelerations[i], 0.0f);
        }
        return data;
    }
};

struct CellManager
{
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates

    // GPU buffer objects
    // static constexpr int BUFFER_COUNT = 5; // IMPORTANT: Remember to update this when adding or removing buffers!!!
    // GLuint buffers[BUFFER_COUNT];
    GLuint positionBuffer = 0;
    GLuint velocityBuffer = 0;
    GLuint accelerationBuffer = 0;
    GLuint massBuffer = 0;
    GLuint radiusBuffer = 0;

    // GPU buffer groups
    BufferGroup allBuffers;
    BufferGroup instanceBuffers;
    BufferGroup physicsBuffers;
    BufferGroup updateBuffers;

    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // Asynchronous readback system for performance monitoring
    GLuint readbackBuffer = 0;      // Buffer for async GPU->CPU data transfer
    GLsync readbackFence = nullptr; // Sync object for async operations
    bool readbackInProgress = false;
    float readbackCooldown = 0.0f; // Timer to limit readback frequency

    // Compute shaders
    Shader *physicsShader = nullptr;
    Shader *updateShader = nullptr;
    Shader *extractShader = nullptr;            // For extracting instance data efficiently
                                                // CPU-side storage using pure SoA (eliminates AoS conversions)
    CellDataSoA cellDataSoA;                    // Primary SoA storage (for debug/analysis)
    CellDataSoA stagingDataSoA;                 // SoA staging buffer for batch operations
    std::vector<ComputeCell> cellStagingBuffer; // Legacy AoS staging (for compatibility)
    int cellCount{0};                           // Current active cell count
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

    void initializeGPUBuffers();
    void bindAllBuffers();
    void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader &cellShader, class Camera &camera);

    // Deprecated AoS methods (for backward compatibility only)
    void addCellsToGPUBuffer(const std::vector<ComputeCell> &cells);
    void addCellToGPUBuffer(const ComputeCell &newCell);
    void addCellToStagingBuffer(const ComputeCell &newCell);

    // Primary SoA-optimized methods (preferred)
    void addCellsToGPUBufferSoA(const CellDataSoA &cellData);
    void addCellToStagingBufferSoA(const glm::vec3 &position, const glm::vec3 &velocity,
                                   const glm::vec3 &acceleration, float mass, float radius);
    void addStagedCellsToGPUBuffer();
    void addStagedCellsToGPUBufferSoA(); // Simplified interface using SoA directly (preferred)
    void addCell(const glm::vec3 &pos, const glm::vec3 &vel, float mass, float radius)
    {
        addCellToStagingBufferSoA(pos, vel, glm::vec3(0.0f), mass, radius);
    }

    // Legacy AoS interface (deprecated - use SoA version above)
    void addCell(const ComputeCell &newCell) { addCellToStagingBuffer(newCell); }

    void updateCells(float deltaTime);
    void cleanup();

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
    void cleanupReadbackSystem(); // SoA data access methods
    const CellDataSoA &getCellDataSoA() const { return cellDataSoA; }
    CellDataSoA &getCellDataSoA() { return cellDataSoA; }
    const CellDataSoA &getStagingDataSoA() const { return stagingDataSoA; }
    CellDataSoA &getStagingDataSoA() { return stagingDataSoA; }

    // Direct SoA data access (more efficient than AoS conversion)
    bool getCellDataSoA(int index, glm::vec3 &position, glm::vec3 &velocity, glm::vec3 &acceleration, float &mass, float &radius) const;
    void updateCellDataSoA(int index, const glm::vec3 &position, const glm::vec3 &velocity, const glm::vec3 &acceleration, float mass, float radius);

    // Batch operations for SoA
    void reserveCells(size_t count)
    {
        cellDataSoA.reserve(count);
        stagingDataSoA.reserve(count);
    }
    void clearStagedCells()
    {
        stagingDataSoA.clear();
        cellStagingBuffer.clear(); // Clear legacy buffer too
        pendingCellCount = 0;
    }
    void commitStagedCells()
    {
        if (stagingDataSoA.size() > 0)
        {
            uploadSoADataDirectly(stagingDataSoA);
            stagingDataSoA.clear();
            pendingCellCount = 0;
        }
    }

    // Direct SoA GPU upload (most efficient)
    void uploadSoADataDirectly(const CellDataSoA &data);

    // Performance comparison utilities
    void spawnCellsLegacy(int count); // Old AoS method for comparison

    // Memory usage calculations for SoA
    size_t getSoAMemoryUsage() const
    {
        return cellCount * (sizeof(glm::vec3) * 3 + sizeof(float) * 2); // 3 vec3s + 2 floats per cell
    }

    size_t getStagingSoAMemoryUsage() const
    {
        return stagingDataSoA.size() * (sizeof(glm::vec3) * 3 + sizeof(float) * 2);
    }

private:
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
};

/*
 * SoA (Structure of Arrays) Refactoring Summary:
 *
 * This codebase has been refactored to use SoA for optimal performance:
 *
 * Primary Data Flow (Optimized):
 * 1. stagingDataSoA -> uploadSoADataDirectly() -> GPU buffers
 *
 * Legacy Data Flow (Compatibility):
 * 2. cellStagingBuffer -> addCellsToGPUBuffer() -> conversion -> GPU buffers
 *
 * Key Benefits:
 * - Eliminated SoA -> AoS -> SoA conversion chain
 * - Better cache locality for batch operations
 * - Direct GPU upload without intermediate conversions
 * - Reduced memory allocations and bandwidth
 *
 * Preferred Methods:
 * - addCellToStagingBufferSoA() instead of addCellToStagingBuffer()
 * - getCellDataSoA() instead of getCellData()
 * - updateCellDataSoA() instead of updateCellData()
 * - uploadSoADataDirectly() instead of addCellsToGPUBufferSoA()
 */
