#pragma once
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "../cell/common_structs.h"

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

    // CPU-side storage for initialization and debugging
    std::vector<ComputeCell> cpuCells;
    std::vector<ComputeCell> cellStagingBuffer;
    
    // Cell count tracking (CPU-side approximation of GPU state)
    int cellCount{0};               // Approximate cell count, may not reflect exact GPU state due to being a frame behind
    int cpuPendingCellCount{0};     // Number of cells pending addition by CPU
    int gpuPendingCellCount{0};     // Approx number of cells pending addition by GPU
    int adhesionCount{ 0 };
    void* mappedPtr = nullptr;      // Pointer to the cell count staging buffer
    GLuint* countPtr = nullptr;     // Typed pointer to the mapped buffer value

    // Configuration
    static constexpr int DEFAULT_CELL_COUNT = 1000;
    int cellLimit = 100000;

    // Constructor and destructor
    CellBufferManager();
    ~CellBufferManager();

    // Buffer management
    void initializeGPUBuffers();
    void cleanup();
    void resetSimulation();
    
    // Cell addition methods
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
    void restoreCellsDirectlyToGPUBuffer(const std::vector<ComputeCell> &cells);
    void setCPUCellData(const std::vector<ComputeCell> &cells);

    // Getters
    int getCellCount() const { return cellCount; }
    void setCellLimit(int limit) { cellLimit = limit; }
    int getCellLimit() const { return cellLimit; }
}; 