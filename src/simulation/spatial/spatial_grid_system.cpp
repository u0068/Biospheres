#include "spatial_grid_system.h"
#include "../../core/config.h"
#include "../../rendering/core/shader_class.h"
#include "../../utils/timer.h"
#include <iostream>
#include <cstring>
#include <cmath>

// System lifecycle
void SpatialGridSystem::initialize() {
    if (initialized) return;
    
    // Initialize cell partitioning buffers (moved from CellManager::initializeSpatialGrid)
    // Create double buffered grid buffers to store cell indices
    glCreateBuffers(1, &gridBuffer);
    glNamedBufferData(gridBuffer,
        TOTAL_GRID_CELLS * MAX_CELLS_PER_GRID * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create double buffered grid count buffers to store number of cells per grid cell
    glCreateBuffers(1, &gridCountBuffer);
    glNamedBufferData(gridCountBuffer,
        TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create double buffered grid offset buffers for prefix sum calculations
    glCreateBuffers(1, &gridOffsetBuffer);
    glNamedBufferData(gridOffsetBuffer,
        TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create hash buffer for sparse grid optimization
    glCreateBuffers(1, &gridHashBuffer);
    glNamedBufferData(gridHashBuffer,
        TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Create active cells buffer for performance optimization
    glCreateBuffers(1, &activeCellsBuffer);
    glNamedBufferData(activeCellsBuffer,
        TOTAL_GRID_CELLS * sizeof(GLuint),
        nullptr, GL_STREAM_COPY);  // Frequently updated by GPU compute shaders

    // Initialize spatial grid shaders (moved from CellManager)
    gridClearShader = new Shader("shaders/spatial/grid_clear.comp");
    gridAssignShader = new Shader("shaders/spatial/grid_assign.comp");
    gridPrefixSumShader = new Shader("shaders/spatial/grid_prefix_sum.comp");
    gridInsertShader = new Shader("shaders/spatial/grid_insert.comp");
    
    // Allocate fluid data arrays on heap to avoid stack overflow
    densityData = std::make_unique<std::array<std::array<std::array<float, GRID_RESOLUTION>, GRID_RESOLUTION>, GRID_RESOLUTION>>();
    velocityData = std::make_unique<std::array<std::array<std::array<glm::vec3, GRID_RESOLUTION>, GRID_RESOLUTION>, GRID_RESOLUTION>>();
    
    // Initialize fluid data to zero
    for (int x = 0; x < GRID_RESOLUTION; ++x) {
        for (int y = 0; y < GRID_RESOLUTION; ++y) {
            for (int z = 0; z < GRID_RESOLUTION; ++z) {
                (*densityData)[x][y][z] = 0.0f;
                (*velocityData)[x][y][z] = glm::vec3(0.0f);
            }
        }
    }
    
    // Initialize fluid textures (stubs for now)
    initializeFluidTextures();
    
    std::cout << "SpatialGridSystem: Initialized spatial grid with " << TOTAL_GRID_CELLS
        << " grid cells (" << GRID_RESOLUTION << "^3)\n";
    std::cout << "SpatialGridSystem: Grid cell size: " << GRID_CELL_SIZE << "\n";
    std::cout << "SpatialGridSystem: Max cells per grid: " << MAX_CELLS_PER_GRID << "\n";
    
    // Report memory layout for fluid data
    reportMemoryLayout();
    
    initialized = true;
}

void SpatialGridSystem::cleanup() {
    if (!initialized) return;
    
    // Clean up cell partitioning buffers (moved from CellManager::cleanupSpatialGrid)
    if (gridBuffer != 0) {
        glDeleteBuffers(1, &gridBuffer);
        gridBuffer = 0;
    }
    if (gridCountBuffer != 0) {
        glDeleteBuffers(1, &gridCountBuffer);
        gridCountBuffer = 0;
    }
    if (gridOffsetBuffer != 0) {
        glDeleteBuffers(1, &gridOffsetBuffer);
        gridOffsetBuffer = 0;
    }
    if (gridHashBuffer != 0) {
        glDeleteBuffers(1, &gridHashBuffer);
        gridHashBuffer = 0;
    }
    if (activeCellsBuffer != 0) {
        glDeleteBuffers(1, &activeCellsBuffer);
        activeCellsBuffer = 0;
    }
    
    // Cleanup spatial grid shaders
    if (gridClearShader) {
        gridClearShader->destroy();
        delete gridClearShader;
        gridClearShader = nullptr;
    }
    if (gridAssignShader) {
        gridAssignShader->destroy();
        delete gridAssignShader;
        gridAssignShader = nullptr;
    }
    if (gridPrefixSumShader) {
        gridPrefixSumShader->destroy();
        delete gridPrefixSumShader;
        gridPrefixSumShader = nullptr;
    }
    if (gridInsertShader) {
        gridInsertShader->destroy();
        delete gridInsertShader;
        gridInsertShader = nullptr;
    }
    
    // Cleanup fluid textures
    cleanupFluidTextures();
    
    initialized = false;
}

void SpatialGridSystem::update(float deltaTime) {
    // TODO: Implement system update logic
}

// Coordinate conversion (unified for all systems)
glm::ivec3 SpatialGridSystem::worldToGrid(const glm::vec3& worldPos) const {
    // Unified coordinate conversion for both cells and fluids
    float halfWorldSize = WORLD_SIZE * 0.5f;
    glm::vec3 normalized = (worldPos + glm::vec3(halfWorldSize)) / WORLD_SIZE;
    glm::ivec3 gridPos = glm::ivec3(normalized * float(GRID_RESOLUTION - 1));
    return glm::clamp(gridPos, glm::ivec3(0), glm::ivec3(GRID_RESOLUTION - 1));
}

glm::vec3 SpatialGridSystem::gridToWorld(const glm::ivec3& gridPos) const {
    // Unified coordinate conversion for both cells and fluids
    glm::vec3 normalized = glm::vec3(gridPos) / float(GRID_RESOLUTION - 1);
    float halfWorldSize = WORLD_SIZE * 0.5f;
    return normalized * WORLD_SIZE - glm::vec3(halfWorldSize);
}

bool SpatialGridSystem::isInsideWorldSphere(const glm::vec3& worldPos) const {
    float distance = glm::length(worldPos - worldSphereCenter);
    return distance <= worldSphereRadius;
}

bool SpatialGridSystem::isValidGridPosition(const glm::ivec3& gridPos) const {
    return gridPos.x >= 0 && gridPos.x < GRID_RESOLUTION &&
           gridPos.y >= 0 && gridPos.y < GRID_RESOLUTION &&
           gridPos.z >= 0 && gridPos.z < GRID_RESOLUTION;
}

// Cell partitioning interface (for CellManager)
void SpatialGridSystem::updateCellGrid(GLuint cellBuffer, int cellCount, GLuint gpuCellCountBuffer) {
    if (cellCount == 0) return;
    
    TimerGPU timer("Spatial Grid Update");

    // ============= PERFORMANCE OPTIMIZATIONS FOR 100K CELLS =============
    // 1. Increased grid resolution from 32³ to 64³ (262,144 grid cells)
    // 2. Reduced max cells per grid from 64 to 32 for better memory access
    // 3. Implemented proper parallel prefix sum with shared memory
    // 4. Optimized work group sizes from 64 to 256 for better GPU utilization
    // 5. Reduced memory barriers and improved dispatch efficiency
    // 6. Added early termination in physics neighbor search
    // ====================================================================

    // HIGHLY OPTIMIZED: Combined operations with minimal barriers
    // Step 1: Clear grid counts and assign cells in parallel
    runGridClear();
    runGridAssign(cellBuffer, cellCount, gpuCellCountBuffer);

    // Single barrier after clear and assign - these operations can overlap
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    // Step 2: Calculate prefix sum for offsets (proper implementation now)
    runGridPrefixSum();

    // Step 3: Insert cells into grid (depends on prefix sum results)
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

    runGridInsert(cellBuffer, cellCount, gpuCellCountBuffer);

    // Add final barrier but don't flush - let caller decide when to flush
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

// Fluid data interface
float SpatialGridSystem::getDensity(const glm::ivec3& gridPos) const {
    if (!isValidGridPosition(gridPos)) return 0.0f;
    return (*densityData)[gridPos.x][gridPos.y][gridPos.z];
}

void SpatialGridSystem::setDensity(const glm::ivec3& gridPos, float density) {
    if (!isValidGridPosition(gridPos)) return;
    (*densityData)[gridPos.x][gridPos.y][gridPos.z] = density;
}

glm::vec3 SpatialGridSystem::getVelocity(const glm::ivec3& gridPos) const {
    if (!isValidGridPosition(gridPos)) return glm::vec3(0.0f);
    return (*velocityData)[gridPos.x][gridPos.y][gridPos.z];
}

void SpatialGridSystem::setVelocity(const glm::ivec3& gridPos, const glm::vec3& velocity) {
    if (!isValidGridPosition(gridPos)) return;
    (*velocityData)[gridPos.x][gridPos.y][gridPos.z] = velocity;
}

// Optimized bulk data access methods for better performance
void SpatialGridSystem::getDensityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, std::vector<float>& output) {
    // Validate bounds
    glm::ivec3 clampedMin = glm::max(minPos, glm::ivec3(0));
    glm::ivec3 clampedMax = glm::min(maxPos, glm::ivec3(GRID_RESOLUTION - 1));
    
    if (clampedMin.x > clampedMax.x || clampedMin.y > clampedMax.y || clampedMin.z > clampedMax.z) {
        output.clear();
        return;
    }
    
    // Calculate output size and reserve memory
    int width = clampedMax.x - clampedMin.x + 1;
    int height = clampedMax.y - clampedMin.y + 1;
    int depth = clampedMax.z - clampedMin.z + 1;
    output.resize(width * height * depth);
    
    // Copy data with optimal memory access pattern
    int index = 0;
    for (int z = clampedMin.z; z <= clampedMax.z; ++z) {
        for (int y = clampedMin.y; y <= clampedMax.y; ++y) {
            for (int x = clampedMin.x; x <= clampedMax.x; ++x) {
                output[index++] = (*densityData)[x][y][z];
            }
        }
    }
}

void SpatialGridSystem::setDensityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, const std::vector<float>& input) {
    // Validate bounds
    glm::ivec3 clampedMin = glm::max(minPos, glm::ivec3(0));
    glm::ivec3 clampedMax = glm::min(maxPos, glm::ivec3(GRID_RESOLUTION - 1));
    
    if (clampedMin.x > clampedMax.x || clampedMin.y > clampedMax.y || clampedMin.z > clampedMax.z) {
        return;
    }
    
    // Calculate expected size
    int width = clampedMax.x - clampedMin.x + 1;
    int height = clampedMax.y - clampedMin.y + 1;
    int depth = clampedMax.z - clampedMin.z + 1;
    int expectedSize = width * height * depth;
    
    if (input.size() != expectedSize) {
        std::cerr << "SpatialGridSystem::setDensityRegion: Input size mismatch. Expected " 
                  << expectedSize << ", got " << input.size() << std::endl;
        return;
    }
    
    // Copy data with optimal memory access pattern
    int index = 0;
    for (int z = clampedMin.z; z <= clampedMax.z; ++z) {
        for (int y = clampedMin.y; y <= clampedMax.y; ++y) {
            for (int x = clampedMin.x; x <= clampedMax.x; ++x) {
                (*densityData)[x][y][z] = input[index++];
            }
        }
    }
}

void SpatialGridSystem::getVelocityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, std::vector<glm::vec3>& output) {
    // Validate bounds
    glm::ivec3 clampedMin = glm::max(minPos, glm::ivec3(0));
    glm::ivec3 clampedMax = glm::min(maxPos, glm::ivec3(GRID_RESOLUTION - 1));
    
    if (clampedMin.x > clampedMax.x || clampedMin.y > clampedMax.y || clampedMin.z > clampedMax.z) {
        output.clear();
        return;
    }
    
    // Calculate output size and reserve memory
    int width = clampedMax.x - clampedMin.x + 1;
    int height = clampedMax.y - clampedMin.y + 1;
    int depth = clampedMax.z - clampedMin.z + 1;
    output.resize(width * height * depth);
    
    // Copy data with optimal memory access pattern
    int index = 0;
    for (int z = clampedMin.z; z <= clampedMax.z; ++z) {
        for (int y = clampedMin.y; y <= clampedMax.y; ++y) {
            for (int x = clampedMin.x; x <= clampedMax.x; ++x) {
                output[index++] = (*velocityData)[x][y][z];
            }
        }
    }
}

void SpatialGridSystem::setVelocityRegion(const glm::ivec3& minPos, const glm::ivec3& maxPos, const std::vector<glm::vec3>& input) {
    // Validate bounds
    glm::ivec3 clampedMin = glm::max(minPos, glm::ivec3(0));
    glm::ivec3 clampedMax = glm::min(maxPos, glm::ivec3(GRID_RESOLUTION - 1));
    
    if (clampedMin.x > clampedMax.x || clampedMin.y > clampedMax.y || clampedMin.z > clampedMax.z) {
        return;
    }
    
    // Calculate expected size
    int width = clampedMax.x - clampedMin.x + 1;
    int height = clampedMax.y - clampedMin.y + 1;
    int depth = clampedMax.z - clampedMin.z + 1;
    int expectedSize = width * height * depth;
    
    if (input.size() != expectedSize) {
        std::cerr << "SpatialGridSystem::setVelocityRegion: Input size mismatch. Expected " 
                  << expectedSize << ", got " << input.size() << std::endl;
        return;
    }
    
    // Copy data with optimal memory access pattern
    int index = 0;
    for (int z = clampedMin.z; z <= clampedMax.z; ++z) {
        for (int y = clampedMin.y; y <= clampedMax.y; ++y) {
            for (int x = clampedMin.x; x <= clampedMax.x; ++x) {
                (*velocityData)[x][y][z] = input[index++];
            }
        }
    }
}

// Fluid injection operations - Task 7.1: Spherical injection algorithms
void SpatialGridSystem::injectDensity(const glm::vec3& worldPos, float radius, float strength) {
    // Requirement 4.1: Support injection at specified coordinates with configurable radius and strength
    // Requirement 4.4: Provide falloff calculation so injection decreases with distance from center
    // Requirement 4.5: Skip injection operations for voxels outside World_Sphere boundaries
    
    // Task 7.2: Parameter validation with bounds checking
    if (!validateInjectionPosition(worldPos)) return;
    if (!validateInjectionRadius(radius)) return;
    if (!validateInjectionStrength(strength)) return;
    
    // Skip zero strength injections (optimization)
    if (strength == 0.0f) return;
    
    // World sphere culling - skip if injection center is outside world sphere
    if (!isInsideWorldSphere(worldPos)) return;
    
    // Convert world position to grid coordinates
    glm::ivec3 centerGrid = worldToGrid(worldPos);
    
    // Calculate voxel size in world units
    float voxelSize = WORLD_SIZE / GRID_RESOLUTION;
    
    // Calculate grid radius - how many voxels the injection sphere covers
    int gridRadius = static_cast<int>(std::ceil(radius / voxelSize));
    
    // Calculate bounding box in grid coordinates
    glm::ivec3 minBounds = glm::max(centerGrid - glm::ivec3(gridRadius), glm::ivec3(0));
    glm::ivec3 maxBounds = glm::min(centerGrid + glm::ivec3(gridRadius), glm::ivec3(GRID_RESOLUTION - 1));
    
    // Iterate through all voxels in the bounding box
    for (int x = minBounds.x; x <= maxBounds.x; ++x) {
        for (int y = minBounds.y; y <= maxBounds.y; ++y) {
            for (int z = minBounds.z; z <= maxBounds.z; ++z) {
                glm::ivec3 gridPos(x, y, z);
                
                // Convert grid position back to world coordinates for distance calculation
                glm::vec3 voxelWorld = gridToWorld(gridPos);
                
                // World sphere culling - skip voxels outside world sphere
                if (!isInsideWorldSphere(voxelWorld)) continue;
                
                // Calculate distance from injection center to voxel center
                float distance = glm::length(voxelWorld - worldPos);
                
                // Task 7.2: Optimized falloff calculation with early termination
                float falloff = calculateOptimizedFalloff(distance, radius);
                if (falloff == 0.0f) continue; // Early termination optimization
                
                float injection = strength * falloff;
                
                // Additive injection - add to existing density
                (*densityData)[x][y][z] += injection;
            }
        }
    }
    
    // Upload updated density data to GPU
    uploadDensityToGPU();
}

void SpatialGridSystem::injectVelocity(const glm::vec3& worldPos, float radius, const glm::vec3& velocity, float strength) {
    // Requirement 4.1: Support injection at specified coordinates with configurable radius and strength
    // Requirement 4.2: Provide additive injection modes for both density and velocity
    // Requirement 4.4: Provide falloff calculation so injection decreases with distance from center
    // Requirement 4.5: Skip injection operations for voxels outside World_Sphere boundaries
    
    // Task 7.2: Parameter validation with bounds checking
    if (!validateInjectionPosition(worldPos)) return;
    if (!validateInjectionRadius(radius)) return;
    if (!validateInjectionStrength(strength)) return;
    
    // Skip zero strength injections (optimization)
    if (strength == 0.0f) return;
    
    // Validate velocity vector for NaN/infinite values
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y) || !std::isfinite(velocity.z)) {
        std::cerr << "SpatialGridSystem: Velocity vector contains non-finite values: (" 
                  << velocity.x << ", " << velocity.y << ", " << velocity.z << ")" << std::endl;
        return;
    }
    
    // World sphere culling - skip if injection center is outside world sphere
    if (!isInsideWorldSphere(worldPos)) return;
    
    // Convert world position to grid coordinates
    glm::ivec3 centerGrid = worldToGrid(worldPos);
    
    // Calculate voxel size in world units
    float voxelSize = WORLD_SIZE / GRID_RESOLUTION;
    
    // Calculate grid radius - how many voxels the injection sphere covers
    int gridRadius = static_cast<int>(std::ceil(radius / voxelSize));
    
    // Calculate bounding box in grid coordinates
    glm::ivec3 minBounds = glm::max(centerGrid - glm::ivec3(gridRadius), glm::ivec3(0));
    glm::ivec3 maxBounds = glm::min(centerGrid + glm::ivec3(gridRadius), glm::ivec3(GRID_RESOLUTION - 1));
    
    // Iterate through all voxels in the bounding box
    for (int x = minBounds.x; x <= maxBounds.x; ++x) {
        for (int y = minBounds.y; y <= maxBounds.y; ++y) {
            for (int z = minBounds.z; z <= maxBounds.z; ++z) {
                glm::ivec3 gridPos(x, y, z);
                
                // Convert grid position back to world coordinates for distance calculation
                glm::vec3 voxelWorld = gridToWorld(gridPos);
                
                // World sphere culling - skip voxels outside world sphere
                if (!isInsideWorldSphere(voxelWorld)) continue;
                
                // Calculate distance from injection center to voxel center
                float distance = glm::length(voxelWorld - worldPos);
                
                // Task 7.2: Optimized falloff calculation with early termination
                float falloff = calculateOptimizedFalloff(distance, radius);
                if (falloff == 0.0f) continue; // Early termination optimization
                
                glm::vec3 injection = velocity * strength * falloff;
                
                // Additive injection - add to existing velocity
                (*velocityData)[x][y][z] += injection;
                

            }
        }
    }
    
    // Upload updated velocity data to GPU
    uploadVelocityToGPU();
}

void SpatialGridSystem::clearAllFluidData() {
    // Requirement 4.2: Provide clear all functionality for resetting fluid state
    
    // Clear all fluid data to zero - optimized loop order for cache efficiency
    for (int x = 0; x < GRID_RESOLUTION; ++x) {
        for (int y = 0; y < GRID_RESOLUTION; ++y) {
            for (int z = 0; z < GRID_RESOLUTION; ++z) {
                (*densityData)[x][y][z] = 0.0f;
                (*velocityData)[x][y][z] = glm::vec3(0.0f);
            }
        }
    }
    
    // Upload cleared data to GPU
    uploadDensityToGPU();
    uploadVelocityToGPU();
}

// GPU synchronization - Upload fluid data from system RAM to GPU textures
void SpatialGridSystem::uploadDensityToGPU() {
    if (densityTexture3D == 0) {
        std::cerr << "SpatialGridSystem::uploadDensityToGPU: Density texture not initialized" << std::endl;
        return;
    }
    
    // CRITICAL FIX: Array layout is [x][y][z] where z varies fastest in memory,
    // but OpenGL expects x to vary fastest. We must repack the data.
    std::vector<float> repackedData;
    repackedData.reserve(GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION);
    
    // Repack data with correct axis order for OpenGL (x varies fastest)
    for (int z = 0; z < GRID_RESOLUTION; ++z) {
        for (int y = 0; y < GRID_RESOLUTION; ++y) {
            for (int x = 0; x < GRID_RESOLUTION; ++x) {
                repackedData.push_back((*densityData)[x][y][z]);
            }
        }
    }
    
    // Bind density texture and upload repacked data
    glBindTexture(GL_TEXTURE_3D, densityTexture3D);
    
    // Upload entire density array to GPU texture
    glTexSubImage3D(GL_TEXTURE_3D, 0, 
                    0, 0, 0,  // offset (x,y,z)
                    GRID_RESOLUTION, GRID_RESOLUTION, GRID_RESOLUTION,  // size (width,height,depth)
                    GL_RED, GL_FLOAT, repackedData.data());
    
    // Check for upload errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "SpatialGridSystem::uploadDensityToGPU: Upload failed. OpenGL error: " << error << std::endl;
    }
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_3D, 0);
}

void SpatialGridSystem::uploadVelocityToGPU() {
    if (velocityTexture3D == 0) {
        std::cerr << "SpatialGridSystem::uploadVelocityToGPU: Velocity texture not initialized" << std::endl;
        return;
    }
    
    // CRITICAL FIX: Array layout is [x][y][z] where z varies fastest in memory,
    // but OpenGL expects x to vary fastest. We must repack the data.
    std::vector<glm::vec3> repackedData;
    repackedData.reserve(GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION);
    
    // Repack data with correct axis order for OpenGL (x varies fastest)
    for (int z = 0; z < GRID_RESOLUTION; ++z) {
        for (int y = 0; y < GRID_RESOLUTION; ++y) {
            for (int x = 0; x < GRID_RESOLUTION; ++x) {
                repackedData.push_back((*velocityData)[x][y][z]);
            }
        }
    }
    
    // Bind velocity texture and upload repacked data
    glBindTexture(GL_TEXTURE_3D, velocityTexture3D);
    
    // Upload entire velocity array to GPU texture
    glTexSubImage3D(GL_TEXTURE_3D, 0, 
                    0, 0, 0,  // offset (x,y,z)
                    GRID_RESOLUTION, GRID_RESOLUTION, GRID_RESOLUTION,  // size (width,height,depth)
                    GL_RGB, GL_FLOAT, repackedData.data());
    
    // Check for upload errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "SpatialGridSystem::uploadVelocityToGPU: Upload failed. OpenGL error: " << error << std::endl;
    }
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_3D, 0);
}

void SpatialGridSystem::uploadFluidRegionToGPU(const glm::ivec3& minGrid, const glm::ivec3& maxGrid) {
    // Validate input bounds
    if (!isValidGridPosition(minGrid) || !isValidGridPosition(maxGrid)) {
        std::cerr << "SpatialGridSystem::uploadFluidRegionToGPU: Invalid grid bounds" << std::endl;
        return;
    }
    
    if (minGrid.x > maxGrid.x || minGrid.y > maxGrid.y || minGrid.z > maxGrid.z) {
        std::cerr << "SpatialGridSystem::uploadFluidRegionToGPU: Invalid region (min > max)" << std::endl;
        return;
    }
    
    if (densityTexture3D == 0 || velocityTexture3D == 0) {
        std::cerr << "SpatialGridSystem::uploadFluidRegionToGPU: Textures not initialized" << std::endl;
        return;
    }
    
    // Calculate region dimensions
    int width = maxGrid.x - minGrid.x + 1;
    int height = maxGrid.y - minGrid.y + 1;
    int depth = maxGrid.z - minGrid.z + 1;
    
    // Extract density region data for upload
    // CRITICAL: Loop order must match OpenGL texture layout (x varies fastest)
    std::vector<float> densityRegion;
    densityRegion.reserve(width * height * depth);
    
    for (int z = minGrid.z; z <= maxGrid.z; ++z) {
        for (int y = minGrid.y; y <= maxGrid.y; ++y) {
            for (int x = minGrid.x; x <= maxGrid.x; ++x) {
                densityRegion.push_back((*densityData)[x][y][z]);
            }
        }
    }
    
    // Upload density region
    glBindTexture(GL_TEXTURE_3D, densityTexture3D);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 
                    minGrid.x, minGrid.y, minGrid.z,  // offset
                    width, height, depth,              // size
                    GL_RED, GL_FLOAT, densityRegion.data());
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "SpatialGridSystem::uploadFluidRegionToGPU: Density region upload failed. OpenGL error: " << error << std::endl;
    }
    
    // Extract velocity region data for upload
    // CRITICAL: Loop order must match OpenGL texture layout (x varies fastest)
    std::vector<glm::vec3> velocityRegion;
    velocityRegion.reserve(width * height * depth);
    
    for (int z = minGrid.z; z <= maxGrid.z; ++z) {
        for (int y = minGrid.y; y <= maxGrid.y; ++y) {
            for (int x = minGrid.x; x <= maxGrid.x; ++x) {
                velocityRegion.push_back((*velocityData)[x][y][z]);
            }
        }
    }
    
    // Upload velocity region
    glBindTexture(GL_TEXTURE_3D, velocityTexture3D);
    glTexSubImage3D(GL_TEXTURE_3D, 0, 
                    minGrid.x, minGrid.y, minGrid.z,  // offset
                    width, height, depth,              // size
                    GL_RGB, GL_FLOAT, velocityRegion.data());
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "SpatialGridSystem::uploadFluidRegionToGPU: Velocity region upload failed. OpenGL error: " << error << std::endl;
    }
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_3D, 0);
}

// Memory usage reporting
size_t SpatialGridSystem::getSystemRAMUsage() const {
    size_t densitySize = sizeof(densityData);
    size_t velocitySize = sizeof(velocityData);
    size_t fluidData = densitySize + velocitySize;
    
    // Validate memory layout for 16-byte alignment optimization
    static_assert(sizeof(float) == 4, "Float must be 4 bytes for GPU compatibility");
    static_assert(sizeof(glm::vec3) == 12, "glm::vec3 must be 12 bytes for GPU compatibility");
    
    return fluidData;
}

size_t SpatialGridSystem::getGPUMemoryUsage() const {
    size_t cellBuffers = (TOTAL_GRID_CELLS * MAX_CELLS_PER_GRID + // gridBuffer
                         TOTAL_GRID_CELLS * 4) * sizeof(GLuint);  // count, offset, hash, active
    
    // Calculate actual 3D texture memory usage
    size_t totalVoxels = GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION;
    size_t densityTextureSize = totalVoxels * 4;  // GL_R32F = 4 bytes per voxel
    size_t velocityTextureSize = totalVoxels * 12; // GL_RGB32F = 12 bytes per voxel (3 floats)
    size_t fluidTextures = densityTextureSize + velocityTextureSize;
    
    return cellBuffers + fluidTextures;
}

void SpatialGridSystem::reportMemoryLayout() const {
    size_t densitySize = sizeof(densityData);
    size_t velocitySize = sizeof(velocityData);
    size_t totalVoxels = GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION;
    
    // Calculate GPU texture memory usage
    size_t densityTextureSize = totalVoxels * 4;  // GL_R32F
    size_t velocityTextureSize = totalVoxels * 12; // GL_RGB32F
    size_t totalGPUTextures = densityTextureSize + velocityTextureSize;
    
    std::cout << "=== SpatialGridSystem Memory Layout Report ===" << std::endl;
    std::cout << "Grid Resolution: " << GRID_RESOLUTION << "^3 = " << totalVoxels << " voxels" << std::endl;
    std::cout << std::endl;
    std::cout << "System RAM Usage:" << std::endl;
    std::cout << "  Density Data: " << (densitySize / (1024 * 1024)) << " MB (" << densitySize << " bytes)" << std::endl;
    std::cout << "  Velocity Data: " << (velocitySize / (1024 * 1024)) << " MB (" << velocitySize << " bytes)" << std::endl;
    std::cout << "  Total Fluid RAM: " << ((densitySize + velocitySize) / (1024 * 1024)) << " MB" << std::endl;
    std::cout << std::endl;
    std::cout << "GPU Texture Usage:" << std::endl;
    std::cout << "  Density Texture (GL_R32F): " << (densityTextureSize / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Velocity Texture (GL_RGB32F): " << (velocityTextureSize / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  Total GPU Textures: " << (totalGPUTextures / (1024 * 1024)) << " MB" << std::endl;
    std::cout << std::endl;
    std::cout << "Memory Efficiency:" << std::endl;
    std::cout << "  Bytes per voxel (RAM): " << (sizeof(float) + sizeof(glm::vec3)) << " bytes (4 + 12)" << std::endl;
    std::cout << "  Bytes per voxel (GPU): " << (4 + 12) << " bytes (4 + 12)" << std::endl;
    std::cout << "  Memory layout: Contiguous 3D arrays optimized for cache locality" << std::endl;
    std::cout << "  Alignment: Data naturally aligned for GPU texture upload" << std::endl;
    std::cout << "  GPU Upload: One-way data flow (no readbacks)" << std::endl;
}

// Cell grid operations (moved from CellManager)
void SpatialGridSystem::runGridClear() {
    gridClearShader->use();

    gridClearShader->setInt("u_totalGridCells", TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridCountBuffer);

    // OPTIMIZED: Use larger work groups for better GPU utilization
    GLuint numGroups = (TOTAL_GRID_CELLS + 255) / 256; // Changed from 64 to 256
    gridClearShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SpatialGridSystem::runGridAssign(GLuint cellBuffer, int cellCount, GLuint gpuCellCountBuffer) {
    gridAssignShader->use();

    gridAssignShader->setInt("u_gridResolution", GRID_RESOLUTION);
    gridAssignShader->setFloat("u_gridCellSize", GRID_CELL_SIZE);
    gridAssignShader->setFloat("u_worldSize", WORLD_SIZE);

    // Use provided cell buffer for spatial grid assignment (matches CellManager exactly)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gpuCellCountBuffer); // Bind GPU cell count buffer

    // OPTIMIZED: Use larger work groups for better memory coalescing
    GLuint numGroups = (cellCount + 255) / 256; // Changed from 64 to 256
    gridAssignShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SpatialGridSystem::runGridPrefixSum() {
    gridPrefixSumShader->use();

    gridPrefixSumShader->setInt("u_totalGridCells", TOTAL_GRID_CELLS);

    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridOffsetBuffer);

    // OPTIMIZED: Use 256-sized work groups to match shader implementation
    GLuint numGroups = (TOTAL_GRID_CELLS + 255) / 256; // Changed from 64 to 256
    gridPrefixSumShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void SpatialGridSystem::runGridInsert(GLuint cellBuffer, int cellCount, GLuint gpuCellCountBuffer) {
    gridInsertShader->use();
    
    gridInsertShader->setInt("u_gridResolution", GRID_RESOLUTION);
    gridInsertShader->setFloat("u_gridCellSize", GRID_CELL_SIZE);
    gridInsertShader->setFloat("u_worldSize", WORLD_SIZE);
    gridInsertShader->setInt("u_maxCellsPerGrid", MAX_CELLS_PER_GRID);
    
    // Use provided cell buffer for spatial grid insertion (matches CellManager exactly)
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, cellBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, gridBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, gridOffsetBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, gridCountBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, gpuCellCountBuffer); // Bind GPU cell count buffer

    // OPTIMIZED: Use larger work groups for better memory coalescing
    GLuint numGroups = (cellCount + 255) / 256; // Changed from 64 to 256
    gridInsertShader->dispatch(numGroups, 1, 1);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

// Fluid operations - 3D texture initialization for GPU access
void SpatialGridSystem::initializeFluidTextures() {
    // Create density 3D texture (GL_R32F format for single float values)
    glGenTextures(1, &densityTexture3D);
    glBindTexture(GL_TEXTURE_3D, densityTexture3D);
    
    // Allocate texture storage with GL_R32F internal format
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, 
                 GRID_RESOLUTION, GRID_RESOLUTION, GRID_RESOLUTION, 
                 0, GL_RED, GL_FLOAT, nullptr);
    
    // Configure texture parameters for trilinear interpolation
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    // Check for OpenGL errors after density texture creation
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "SpatialGridSystem: Failed to create density 3D texture. OpenGL error: " << error << std::endl;
        glDeleteTextures(1, &densityTexture3D);
        densityTexture3D = 0;
        return;
    }
    
    // Create velocity 3D texture (GL_RGB32F format for vec3 values)
    glGenTextures(1, &velocityTexture3D);
    glBindTexture(GL_TEXTURE_3D, velocityTexture3D);
    
    // Allocate texture storage with GL_RGB32F internal format
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB32F, 
                 GRID_RESOLUTION, GRID_RESOLUTION, GRID_RESOLUTION, 
                 0, GL_RGB, GL_FLOAT, nullptr);
    
    // Configure texture parameters for trilinear interpolation
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    
    // Check for OpenGL errors after velocity texture creation
    error = glGetError();
    if (error != GL_NO_ERROR) {
        std::cerr << "SpatialGridSystem: Failed to create velocity 3D texture. OpenGL error: " << error << std::endl;
        glDeleteTextures(1, &velocityTexture3D);
        velocityTexture3D = 0;
        // Also cleanup density texture on failure
        if (densityTexture3D != 0) {
            glDeleteTextures(1, &densityTexture3D);
            densityTexture3D = 0;
        }
        return;
    }
    
    // Unbind texture to prevent state pollution
    glBindTexture(GL_TEXTURE_3D, 0);
    
    std::cout << "SpatialGridSystem: Initialized fluid 3D textures:" << std::endl;
    std::cout << "  - Density texture (GL_R32F): " << GRID_RESOLUTION << "^3 = " 
              << (GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION * 4 / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  - Velocity texture (GL_RGB32F): " << GRID_RESOLUTION << "^3 = " 
              << (GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION * 12 / (1024 * 1024)) << " MB" << std::endl;
    std::cout << "  - Total GPU texture memory: " 
              << (GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION * 16 / (1024 * 1024)) << " MB" << std::endl;
}

void SpatialGridSystem::cleanupFluidTextures() {
    // Cleanup density 3D texture
    if (densityTexture3D != 0) {
        glDeleteTextures(1, &densityTexture3D);
        densityTexture3D = 0;

    }
    
    // Cleanup velocity 3D texture
    if (velocityTexture3D != 0) {
        glDeleteTextures(1, &velocityTexture3D);
        velocityTexture3D = 0;
        std::cout << "SpatialGridSystem: Cleaned up velocity 3D texture" << std::endl;
    }
}

// Temporary stub implementations for CellManager compatibility
// These will be removed once the migration is complete

// Note: These are temporary stubs to prevent compilation errors during the transition.
// The actual implementations will be moved from CellManager in the next task.

// Validation function to test the system
bool SpatialGridSystem::validateSystem() const {
    if (!initialized) {
        std::cout << "SpatialGridSystem: System not initialized" << std::endl;
        return false;
    }
    
    // Validate grid buffers
    if (gridBuffer == 0 || gridCountBuffer == 0 || gridOffsetBuffer == 0) {
        std::cout << "SpatialGridSystem: Grid buffers not properly initialized" << std::endl;
        return false;
    }
    
    // Validate fluid textures
    if (densityTexture3D == 0 || velocityTexture3D == 0) {
        std::cout << "SpatialGridSystem: Fluid 3D textures not properly initialized" << std::endl;
        return false;
    }
    
    // Validate shaders
    if (!gridClearShader || !gridAssignShader || !gridPrefixSumShader || !gridInsertShader) {
        std::cout << "SpatialGridSystem: Grid shaders not properly initialized" << std::endl;
        return false;
    }
    
    // Validate memory layout
    if (!validateMemoryLayout()) {
        std::cout << "SpatialGridSystem: Memory layout validation failed" << std::endl;
        return false;
    }
    
    std::cout << "SpatialGridSystem: System validation passed" << std::endl;
    return true;
}

bool SpatialGridSystem::validateMemoryLayout() const {
    // Validate data type sizes for GPU compatibility
    if (sizeof(float) != 4) {
        std::cout << "SpatialGridSystem: Float size is " << sizeof(float) << " bytes, expected 4" << std::endl;
        return false;
    }
    
    if (sizeof(glm::vec3) != 12) {
        std::cout << "SpatialGridSystem: glm::vec3 size is " << sizeof(glm::vec3) << " bytes, expected 12" << std::endl;
        return false;
    }
    
    // Validate array dimensions
    size_t expectedDensitySize = GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION * sizeof(float);
    size_t expectedVelocitySize = GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION * sizeof(glm::vec3);
    
    if (sizeof(densityData) != expectedDensitySize) {
        std::cout << "SpatialGridSystem: Density array size mismatch. Expected " << expectedDensitySize 
                  << ", got " << sizeof(densityData) << std::endl;
        return false;
    }
    
    if (sizeof(velocityData) != expectedVelocitySize) {
        std::cout << "SpatialGridSystem: Velocity array size mismatch. Expected " << expectedVelocitySize 
                  << ", got " << sizeof(velocityData) << std::endl;
        return false;
    }
    
    // Validate memory alignment for optimal GPU upload
    // The arrays should be naturally aligned for efficient texture upload
    uintptr_t densityAddr = reinterpret_cast<uintptr_t>(&(*densityData)[0][0][0]);
    uintptr_t velocityAddr = reinterpret_cast<uintptr_t>(&(*velocityData)[0][0][0]);
    
    if (densityAddr % 16 != 0) {
        std::cout << "SpatialGridSystem: Density data not 16-byte aligned (address: 0x" 
                  << std::hex << densityAddr << std::dec << ")" << std::endl;
        return false;
    }
    
    if (velocityAddr % 16 != 0) {
        std::cout << "SpatialGridSystem: Velocity data not 16-byte aligned (address: 0x" 
                  << std::hex << velocityAddr << std::dec << ")" << std::endl;
        return false;
    }
    
    std::cout << "SpatialGridSystem: Memory layout validation passed" << std::endl;
    std::cout << "  - Density array: " << (sizeof(densityData) / (1024 * 1024)) << " MB, 16-byte aligned" << std::endl;
    std::cout << "  - Velocity array: " << (sizeof(velocityData) / (1024 * 1024)) << " MB, 16-byte aligned" << std::endl;
    
    return true;
}

// Injection parameter validation methods - Task 7.2
bool SpatialGridSystem::validateInjectionRadius(float radius) const {
    // Requirement 4.1: Implement bounds checking for injection radius
    // Requirement 4.4: Validate injection parameters
    
    // Minimum radius should be at least one voxel size
    float minRadius = WORLD_SIZE / GRID_RESOLUTION; // One voxel size
    
    // Maximum radius should not exceed half the world size to prevent excessive computation
    float maxRadius = WORLD_SIZE * 0.5f;
    
    if (radius < minRadius) {
        std::cerr << "SpatialGridSystem: Injection radius " << radius 
                  << " is too small. Minimum: " << minRadius << std::endl;
        return false;
    }
    
    if (radius > maxRadius) {
        std::cerr << "SpatialGridSystem: Injection radius " << radius 
                  << " is too large. Maximum: " << maxRadius << std::endl;
        return false;
    }
    
    return true;
}

bool SpatialGridSystem::validateInjectionStrength(float strength) const {
    // Requirement 4.1: Implement bounds checking for injection strength
    // Requirement 4.4: Validate injection parameters
    
    // Allow negative strength for subtraction/removal operations
    float maxStrength = 1000.0f; // Reasonable upper bound to prevent overflow
    
    if (std::abs(strength) > maxStrength) {
        std::cerr << "SpatialGridSystem: Injection strength " << strength 
                  << " exceeds maximum allowed: " << maxStrength << std::endl;
        return false;
    }
    
    // Check for NaN or infinite values
    if (!std::isfinite(strength)) {
        std::cerr << "SpatialGridSystem: Injection strength is not finite: " << strength << std::endl;
        return false;
    }
    
    return true;
}

bool SpatialGridSystem::validateInjectionPosition(const glm::vec3& worldPos) const {
    // Requirement 4.1: Add coordinate validation for injection positions
    // Requirement 4.5: Skip injection operations for voxels outside World_Sphere boundaries
    
    // Check for NaN or infinite values in position
    if (!std::isfinite(worldPos.x) || !std::isfinite(worldPos.y) || !std::isfinite(worldPos.z)) {
        std::cerr << "SpatialGridSystem: Injection position contains non-finite values: (" 
                  << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << std::endl;
        return false;
    }
    
    // Check if position is within world bounds (not just world sphere)
    float halfWorldSize = WORLD_SIZE * 0.5f;
    if (worldPos.x < -halfWorldSize || worldPos.x > halfWorldSize ||
        worldPos.y < -halfWorldSize || worldPos.y > halfWorldSize ||
        worldPos.z < -halfWorldSize || worldPos.z > halfWorldSize) {
        std::cerr << "SpatialGridSystem: Injection position outside world bounds: (" 
                  << worldPos.x << ", " << worldPos.y << ", " << worldPos.z << ")" << std::endl;
        return false;
    }
    
    // Note: World sphere validation is done separately in injection methods
    // This allows injection near the sphere boundary even if center is outside
    
    return true;
}

float SpatialGridSystem::calculateOptimizedFalloff(float distance, float radius) const {
    // Requirement 4.4: Create falloff calculation with early termination optimization
    
    // Early termination - return 0 if outside radius
    if (distance >= radius) return 0.0f;
    
    // Handle center case (avoid division by zero)
    if (distance <= 0.0f) return 1.0f;
    
    // Linear falloff from 1.0 at center to 0.0 at radius
    // This provides smooth, predictable injection behavior
    float falloff = 1.0f - (distance / radius);
    
    // Clamp to ensure valid range (should not be necessary with proper input)
    return glm::clamp(falloff, 0.0f, 1.0f);
}