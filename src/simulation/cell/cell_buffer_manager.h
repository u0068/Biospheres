#pragma once
#include <vector>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "../cell/common_structs.h"
#include "../../rendering/core/mesh/sphere_mesh.h"

// Forward declarations
class Shader;

struct CellBufferManager
{
    // GPU buffer objects - Triple buffered for performance
    GLuint cellBuffer[3]{};         // SSBO for compute cell data (triple buffered)
    GLuint instanceBuffer{};        // VBO for instance rendering data
    int bufferRotation{};

    // Cell count management
    GLuint gpuCellCountBuffer{};     // GPU-accessible cell count buffer
    GLuint stagingCellCountBuffer{}; // CPU-accessible cell count buffer (no sync stalls)
    GLuint cellAdditionBuffer{};     // Cell addition queue for GPU

    // Cell data staging buffer for CPU reads (avoids GPU->CPU transfer warnings)
    GLuint stagingCellBuffer{};      // CPU-accessible cell data buffer
    void* mappedCellPtr = nullptr;   // Pointer to the cell data staging buffer

    // Genome buffer (immutable, no need for double buffering)
    GLuint modeBuffer{};

    // Spatial partitioning buffers - Double buffered
    GLuint gridBuffer{};       // SSBO for grid cell data (stores cell indices)
    GLuint gridCountBuffer{};  // SSBO for grid cell counts
    GLuint gridOffsetBuffer{}; // SSBO for grid cell starting offsets
    
    // PERFORMANCE OPTIMIZATION: Additional buffers for 100k cells
    GLuint gridHashBuffer{};   // Hash-based lookup for sparse grids
    GLuint activeCellsBuffer{}; // Buffer containing only active grid cells
    uint32_t activeGridCount{0}; // Number of active grid cells

    // Sphere mesh for instanced rendering
    SphereMesh sphereMesh;

    // Cell count tracking (CPU-side approximation of GPU state)
    int cellCount{0};               // Approximate cell count, may not reflect exact GPU state due to being a frame behind
    int cpuPendingCellCount{0};     // Number of cells pending addition by CPU
    int gpuPendingCellCount{0};     // Approx number of cells pending addition by GPU
    int adhesionCount{ 0 };
    void* mappedPtr = nullptr;      // Pointer to the cell count staging buffer
    GLuint* countPtr = nullptr;     // Typed pointer to the mapped buffer value

    // Configuration
    static constexpr int DEFAULT_CELL_COUNT = config::DEFAULT_CELL_COUNT;
    float spawnRadius = config::DEFAULT_SPAWN_RADIUS;
    int cellLimit = config::MAX_CELLS;

    // Constructor and destructor
    CellBufferManager();
    ~CellBufferManager();

    // Buffer management functions
    void initializeGPUBuffers();
    void cleanup();
    void resetSimulation();

    // Cell addition functions
    void addCellsToGPUBuffer(const std::vector<ComputeCell> &cells);
    void addCellToGPUBuffer(const ComputeCell &newCell);
    void addCellToStagingBuffer(const ComputeCell &newCell);
    void addCell(const ComputeCell &newCell) { addCellToStagingBuffer(newCell); }
    void addStagedCellsToGPUBuffer();
    void addGenomeToBuffer(GenomeData& genomeData) const;
    void applyCellAdditions();

    // Buffer rotation and access
    int getRotatedIndex(int index, int max) const { return (index + bufferRotation) % max; }
    void rotateBuffers() { bufferRotation = getRotatedIndex(1, 3); }
    GLuint getCellReadBuffer() const { return cellBuffer[getRotatedIndex(0, 3)]; }
    GLuint getCellWriteBuffer() const { return cellBuffer[getRotatedIndex(1, 3)]; }

    // Cell data access
    ComputeCell getCellData(int index) const;
    void updateCellData(int index, const ComputeCell &newData);
    void syncCellPositionsFromGPU();

    // Configuration setters
    void setCellLimit(int limit) { cellLimit = limit; }
    int getCellLimit() const { return cellLimit; }
    int getCellCount() const { return cellCount; }
    float getSpawnRadius() const { return spawnRadius; }

    // Direct buffer restoration (for keyframe restoration)
    void restoreCellsDirectlyToGPUBuffer(const std::vector<ComputeCell> &cells);
    void setCPUCellData(const std::vector<ComputeCell> &cells);

    // CPU-side storage for initialization and debugging
    std::vector<ComputeCell> cpuCells;
    std::vector<ComputeCell> cellStagingBuffer;
}; 