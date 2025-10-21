#pragma once

#include <vector>
#include <array>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../core/config.h"

// Forward declarations
class Shader;

/**
 * SpatialGridSystem - Unified spatial grid management for cells and fluids
 * 
 * This system replaces and extends the spatial grid functionality previously
 * embedded in CellManager. It provides both cell partitioning and fluid data
 * storage with 64³ voxel resolution.
 */
class SpatialGridSystem {
private:
    // Grid Configuration (reuse existing CellManager settings)
    static constexpr int GRID_RESOLUTION = config::GRID_RESOLUTION; // 64
    static constexpr float WORLD_SIZE = config::WORLD_SIZE; // 100.0f
    static constexpr float GRID_CELL_SIZE = config::GRID_CELL_SIZE;
    static constexpr int MAX_CELLS_PER_GRID = config::MAX_CELLS_PER_GRID;
    static constexpr int TOTAL_GRID_CELLS = config::TOTAL_GRID_CELLS;
    
    // System RAM Storage (Primary)
    // Fluid simulation data (new) - heap allocated to avoid stack overflow
    std::unique_ptr<std::array<std::array<std::array<float, GRID_RESOLUTION>, GRID_RESOLUTION>, GRID_RESOLUTION>> densityData;
    std::unique_ptr<std::array<std::array<std::array<glm::vec3, GRID_RESOLUTION>, GRID_RESOLUTION>, GRID_RESOLUTION>> velocityData;
    
    // GPU Resources
    // Cell partitioning buffers (moved from CellManager)
    GLuint gridBuffer{};           // SSBO for grid cell data (stores cell indices)
    GLuint gridCountBuffer{};      // SSBO for grid cell counts
    GLuint gridOffsetBuffer{};     // SSBO for grid cell starting offsets
    GLuint gridHashBuffer{};       // Hash-based lookup for sparse grids
    GLuint activeCellsBuffer{};    // Buffer containing only active grid cells
    
    // Fluid data textures (new)
    GLuint densityTexture3D{};     // GL_R32F format
    GLuint velocityTexture3D{};    // GL_RGB32F format
    
    // Compute shaders
    // Cell partitioning shaders (moved from CellManager)
    Shader* gridClearShader = nullptr;
    Shader* gridAssignShader = nullptr;
    Shader* gridPrefixSumShader = nullptr;
    Shader* gridInsertShader = nullptr;
    
    // Fluid shaders (new - stubs for now)
    Shader* fluidInjectionShader = nullptr;
    Shader* fluidVisualizationShader = nullptr;
    Shader* fluidClearShader = nullptr;
    
    // World sphere culling (reuse existing config)
    glm::vec3 worldSphereCenter{config::SPHERE_CENTER};
    float worldSphereRadius{config::SPHERE_RADIUS};
    
    // System state
    bool initialized = false;

public:
    // System lifecycle
    void initialize();
    void cleanup();
    void update(float deltaTime);
    
    // Coordinate conversion (unified for all systems)
    glm::ivec3 worldToGrid(const glm::vec3& worldPos) const;
    glm::vec3 gridToWorld(const glm::ivec3& gridPos) const;
    bool isInsideWorldSphere(const glm::vec3& worldPos) const;
    bool isValidGridPosition(const glm::ivec3& gridPos) const;
    
    // Cell partitioning interface (for CellManager)
    void updateCellGrid(GLuint cellBuffer, int cellCount, GLuint gpuCellCountBuffer);
    GLuint getCellGridBuffer() const { return gridBuffer; }
    GLuint getCellCountBuffer() const { return gridCountBuffer; }
    GLuint getCellOffsetBuffer() const { return gridOffsetBuffer; }
    GLuint getGridHashBuffer() const { return gridHashBuffer; }
    GLuint getActiveCellsBuffer() const { return activeCellsBuffer; }
    
    // Fluid data interface
    float getDensity(const glm::ivec3& gridPos) const;
    void setDensity(const glm::ivec3& gridPos, float density);
    glm::vec3 getVelocity(const glm::ivec3& gridPos) const;
    void setVelocity(const glm::ivec3& gridPos, const glm::vec3& velocity);
    
    // Optimized bulk data access methods
    void getDensityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, std::vector<float>& output);
    void setDensityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, const std::vector<float>& input);
    void getVelocityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, std::vector<glm::vec3>& output);
    void setVelocityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, const std::vector<glm::vec3>& input);
    
    // Fluid injection operations (stubs for now)
    void injectDensity(const glm::vec3& worldPos, float radius, float strength);
    void injectVelocity(const glm::vec3& worldPos, float radius, const glm::vec3& velocity, float strength);
    void clearAllFluidData();
    
    // GPU synchronization (stubs for now)
    void uploadDensityToGPU();
    void uploadVelocityToGPU();
    void uploadFluidRegionToGPU(const glm::ivec3& minGrid, const glm::ivec3& maxGrid);
    
    // GPU resource access (for rendering and compute)
    GLuint getDensityTexture() const { return densityTexture3D; }
    GLuint getVelocityTexture() const { return velocityTexture3D; }
    
    // Memory usage reporting
    size_t getSystemRAMUsage() const;
    size_t getGPUMemoryUsage() const;
    void reportMemoryLayout() const;
    
    // Validation functions
    bool validateSystem() const;
    bool validateMemoryLayout() const;
    
    // Injection parameter validation (Task 7.2)
    bool validateInjectionRadius(float radius) const;
    bool validateInjectionStrength(float strength) const;
    bool validateInjectionPosition(const glm::vec3& worldPos) const;
    float calculateOptimizedFalloff(float distance, float radius) const;

private:
    // Cell grid operations (moved from CellManager)
    void runGridClear();
    void runGridAssign(GLuint cellBuffer, int cellCount, GLuint gpuCellCountBuffer);
    void runGridPrefixSum();
    void runGridInsert(GLuint cellBuffer, int cellCount, GLuint gpuCellCountBuffer);
    
    // Fluid operations (stubs for now)
    void initializeFluidTextures();
    void cleanupFluidTextures();
};