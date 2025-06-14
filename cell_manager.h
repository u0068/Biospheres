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

// GPU compute cell structure matching the compute shader
struct ComputeCell {
    glm::vec4 positionAndRadius;  // x, y, z, radius
    glm::vec4 velocityAndMass;    // vx, vy, vz, mass
    glm::vec4 acceleration;       // ax, ay, az, unused
};

struct CellManager {
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates
    
    // GPU buffer objects
    //static constexpr int BUFFER_COUNT = 5; // IMPORTANT: Remember to update this when adding or removing buffers!!!
    //GLuint buffers[BUFFER_COUNT];
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
    GLuint readbackBuffer = 0;       // Buffer for async GPU->CPU data transfer
    GLsync readbackFence = nullptr;  // Sync object for async operations
    bool readbackInProgress = false;
    float readbackCooldown = 0.0f;   // Timer to limit readback frequency
    
    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* updateShader = nullptr;
    Shader* extractShader = nullptr;  // For extracting instance data efficiently

    // CPU-side storage for initialization and debugging
	std::vector<ComputeCell> cpuCells; // Deprecated, since we use GPU buffers now. Get rid of this after refactoring.
    std::vector<ComputeCell> cellStagingBuffer;
	int cellCount{ 0 }; // Not sure if this is accurately representative of the GPU state, im gonna need to work on it
	int pendingCellCount{ 0 }; // Number of cells pending addition

    // Configuration
    static constexpr int MAX_CELLS = config::MAX_CELLS;
    static constexpr int DEFAULT_CELL_COUNT = config::DEFAULT_CELL_COUNT;
    float spawnRadius = config::DEFAULT_SPAWN_RADIUS;  // Acts as both spawn area and containment barrier

    // Constructor and destructor
    CellManager();
    ~CellManager();
    
    // We declare functions in the struct, but we will define them in the cell_manager.cpp file.
    // This is because when a file is edited, the compiler will also have to recompile all the files that include it.
    // So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.
    
    void initializeGPUBuffers();
    void bindAllBuffers();
    void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader& cellShader, class Camera& camera);
    void addCellsToGPUBuffer(const std::vector<ComputeCell>& cells);
    void addCellToGPUBuffer(const ComputeCell& newCell);
    void addCellToStagingBuffer(const ComputeCell& newCell);
    void addCell(const ComputeCell& newCell) { addCellToStagingBuffer(newCell); }
    void addStagedCellsToGPUBuffer();
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
    void setActiveCellCount(int count) {
        if (count <= cellCount && count >= 0) {
            // This allows reducing active cells for performance testing
            // without changing the actual cell count or buffer data
        }
    }    // Cell selection and interaction system
    struct SelectedCellInfo {
        int cellIndex = -1;
        ComputeCell cellData;
        bool isValid = false;
        glm::vec3 dragOffset = glm::vec3(0.0f); // Offset from cell center when dragging starts
        float dragDistance = 10.0f; // Distance from camera to maintain during dragging
    };
    
    SelectedCellInfo selectedCell;
    bool isDraggingCell = false;

    // Selection and interaction functions
    void handleMouseInput(const glm::vec2& mousePos, const glm::vec2& screenSize, 
                         const class Camera& camera, bool isMousePressed, bool isMouseDown, 
                         float scrollDelta = 0.0f);
    int selectCellAtPosition(const glm::vec3& rayOrigin, const glm::vec3& rayDirection);
    void dragSelectedCell(const glm::vec3& newWorldPosition);
    void clearSelection();
    
    // Handle the end of dragging (restore physics)
    void endDrag();

    // GPU synchronization for selection (synchronous readback for immediate use)
    void syncCellPositionsFromGPU();

    // Utility functions for mouse interaction
    glm::vec3 calculateMouseRay(const glm::vec2& mousePos, const glm::vec2& screenSize, 
                               const class Camera& camera);
    bool raySphereIntersection(const glm::vec3& rayOrigin, const glm::vec3& rayDirection,
                              const glm::vec3& sphereCenter, float sphereRadius, float& distance);

    // Getters for selection system
    bool hasSelectedCell() const { return selectedCell.isValid; }
    const SelectedCellInfo& getSelectedCell() const { return selectedCell; }
    ComputeCell getCellData(int index) const;
	void updateCellData(int index, const ComputeCell& newData); // Needs refactoring

    // Asynchronous readback functions for performance monitoring
    void initializeReadbackSystem();
    void updateReadbackSystem(float deltaTime);
    void requestAsyncReadback();
    bool checkAsyncReadback(ComputeCell* outputData, int maxCells);
    void cleanupReadbackSystem();

private:
    void runPhysicsCompute(float deltaTime);
    void runUpdateCompute(float deltaTime);
};
