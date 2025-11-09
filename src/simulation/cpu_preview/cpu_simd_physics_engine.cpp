#include "cpu_simd_physics_engine.h"
#include "cpu_division_inheritance_handler.h"
#include "../../core/config.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <functional>
#include <intrin.h> // For _mm_prefetch on MSVC

CPUSIMDPhysicsEngine::CPUSIMDPhysicsEngine() 
{
    // Always create spatial grid for collision optimization (even in preview mode)
    m_spatialGrid = std::make_unique<CPUSpatialGrid>();
    m_spatialGrid->initialize();
    m_spatialGrid->clear();
    
    // Initialize complete adhesion force calculator (GPU-equivalent)
    m_adhesionCalculator = std::make_unique<CPUAdhesionForceCalculator>();
    
    // Initialize connection management and validation system (Requirements 10.1-10.5, 7.4, 7.5)
    m_connectionManager = std::make_unique<CPUAdhesionConnectionManager>();
    
    // Initialize division inheritance handler for complete adhesion inheritance
    m_divisionInheritanceHandler = std::make_unique<CPUDivisionInheritanceHandler>();
    
    // Initialize SIMD batch processor for adhesion forces (Requirements 5.1-5.5)
    m_simdBatchProcessor = std::make_unique<SIMDAdhesionBatchProcessor>();
    m_simdBatchProcessor->initialize();
    
    // Initialize SIMD performance metrics
    m_simdMetrics = SIMDPerformanceMetrics{};
    
    // Initialize preallocated buffers to avoid dynamic allocation
    m_buffers.initialize();
}

CPUSIMDPhysicsEngine::~CPUSIMDPhysicsEngine() {
    // Cleanup handled by RAII
}

void CPUSIMDPhysicsEngine::setupConnectionManager(CPUCellPhysics_SoA* cellData, CPUAdhesionConnections_SoA* adhesionData) {
    if (m_connectionManager) {
        m_connectionManager->setCellData(cellData);
        m_connectionManager->setAdhesionData(adhesionData);
    }
}

void CPUSIMDPhysicsEngine::simulateStep(CPUCellPhysics_SoA& cells, 
                                       CPUAdhesionConnections_SoA& adhesions,
                                       float deltaTime,
                                       const std::vector<GPUModeAdhesionSettings>& modeSettings) {
    simulateStep(cells, adhesions, deltaTime, modeSettings, nullptr);
}

void CPUSIMDPhysicsEngine::simulateStep(CPUCellPhysics_SoA& cells, 
                                       CPUAdhesionConnections_SoA& adhesions,
                                       float deltaTime,
                                       const std::vector<GPUModeAdhesionSettings>& modeSettings,
                                       const CPUGenomeParameters* genomeParams) {
    m_stepStart = std::chrono::steady_clock::now();
    
    try {
        // Behavioral equivalence with GPU pipeline:
        // 1. cell_physics_spatial.comp - collision forces
        // 2. adhesion_physics.comp - adhesion forces  
        // 3. cell_velocity_update.comp - velocity integration
        // 4. cell_position_update.comp - position integration
        
        // Step 1: Update spatial grid for collision optimization
        updateSpatialGrid(cells);
        
        // Step 2: Calculate collision forces (cell_physics_spatial.comp)
        calculateCollisionForces(cells);
        
        // Step 3: Calculate adhesion forces (adhesion_physics.comp)
        calculateAdhesionForces(cells, adhesions, modeSettings);
        
        // Step 4: Update velocities (cell_velocity_update.comp equivalent)
        updateVelocities(cells, deltaTime);
        
        // Step 5: Update positions (cell_position_update.comp)
        integrateVerlet(cells, deltaTime);
        
        // Step 6: Update orientations (simplified for CPU)
        updateOrientations(cells, deltaTime);
        
        // Step 7: Apply boundary constraints
        applyBoundaryConstraints(cells);
        
        // Step 8: Check for cell division based on age and division threshold
        checkCellDivision(cells, adhesions, deltaTime, genomeParams);
        
        m_processedCellCount = cells.activeCellCount;
        
        // Update performance metrics
        auto stepEnd = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(stepEnd - m_stepStart);
        m_lastStepTime = duration.count() / 1000.0f; // Convert to milliseconds
    }
    catch (const std::exception& e) {
        std::cerr << "CPU SIMD Physics Engine error: " << e.what() << std::endl;
        throw;
    }
}

void CPUSIMDPhysicsEngine::calculateCollisionForces(CPUCellPhysics_SoA& cells) {
    // Reset accelerations using SIMD
    simd_vec3_scale(cells.acc_x.data(), cells.acc_y.data(), cells.acc_z.data(), 0.0f, cells.activeCellCount);

    // Skip collision detection if there's only one cell (major optimization)
    if (cells.activeCellCount <= 1) {
        return;
    }

    // Process collisions in cache-friendly batches for optimal CPU performance
    const size_t batchSize = BLOCK_SIZE; // 32 cells per batch for cache optimization

    // Single-threaded collision detection (multithreading disabled for preview mode)
    // OPTIMIZATION: Use multi-threading for collision detection if enabled and beneficial
    if (false && cells.activeCellCount >= 64) {
        // Determine number of threads to use
        int threadCount = config::collisionThreadCount;
        if (threadCount <= 0) {
            threadCount = std::max(1u, std::thread::hardware_concurrency() / 2); // Use half of available cores
        }
        threadCount = std::min(threadCount, 8); // Cap at 8 threads for diminishing returns

        // Calculate batches per thread
        size_t totalBatches = (cells.activeCellCount + batchSize - 1) / batchSize;
        size_t batchesPerThread = std::max(size_t(1), totalBatches / threadCount);

        std::vector<std::thread> threads;
        threads.reserve(threadCount);

        // Launch worker threads
        for (int t = 0; t < threadCount; ++t) {
            size_t threadStartBatch = t * batchesPerThread;
            size_t threadEndBatch = (t == threadCount - 1) ? totalBatches : (t + 1) * batchesPerThread;

            if (threadStartBatch >= totalBatches) break;

            threads.emplace_back([this, &cells, batchSize, threadStartBatch, threadEndBatch]() {
                for (size_t batch = threadStartBatch; batch < threadEndBatch; ++batch) {
                    size_t batchStart = batch * batchSize;
                    size_t batchCount = std::min(batchSize, cells.activeCellCount - batchStart);
                    processCellBatch(cells, batchStart, batchCount);
                }
            });
        }

        // Wait for all threads to complete
        for (auto& thread : threads) {
            thread.join();
        }
    } else {
        // Single-threaded path for small cell counts
        for (size_t batchStart = 0; batchStart < cells.activeCellCount; batchStart += batchSize) {
            size_t batchCount = std::min(batchSize, cells.activeCellCount - batchStart);
            processCellBatch(cells, batchStart, batchCount);
        }
    }

    // Apply GPU-style acceleration damping (behavioral equivalence)
    applyAccelerationDamping(cells);
}

void CPUSIMDPhysicsEngine::calculateAdhesionForces(CPUCellPhysics_SoA& cells,
                                                   const CPUAdhesionConnections_SoA& adhesions,
                                                   const std::vector<GPUModeAdhesionSettings>& modeSettings) {
    // Skip adhesion forces if no connections (major optimization)
    if (adhesions.activeConnectionCount == 0) {
        return;
    }

    // CRITICAL FIX: Always use non-SIMD path to ensure torques are applied correctly
    // The SIMD batch processor does not handle torque calculations, causing adhesion
    // connections to not apply rotational forces when activeConnectionCount >= 8
    // This ensures consistent behavior regardless of connection count
    if (adhesions.activeConnectionCount > 0) {
        // Reset angular accelerations for torque accumulation
        std::fill(cells.angularAcc_x.begin(), cells.angularAcc_x.begin() + cells.activeCellCount, 0.0f);
        std::fill(cells.angularAcc_y.begin(), cells.angularAcc_y.begin() + cells.activeCellCount, 0.0f);
        std::fill(cells.angularAcc_z.begin(), cells.angularAcc_z.begin() + cells.activeCellCount, 0.0f);

        // Process connections individually using optimized method with full torque support
        for (size_t i = 0; i < adhesions.activeConnectionCount; ++i) {
            if (adhesions.isActive[i] == 0) continue;
            processAdhesionConnection(static_cast<uint32_t>(i), adhesions, cells, modeSettings);
        }
    }
}

void CPUSIMDPhysicsEngine::processAdhesionForcesPerCell(CPUCellPhysics_SoA& cells,
                                                       const CPUAdhesionConnections_SoA& adhesions,
                                                       const std::vector<GPUModeAdhesionSettings>& modeSettings) {
    // Per-cell adhesion processing matching GPU approach (iterate through adhesionIndices[20])
    // This matches the GPU compute shader approach where each cell processes its own adhesions
    
    // Reset angular accelerations for torque accumulation
    std::fill(cells.angularAcc_x.begin(), cells.angularAcc_x.begin() + cells.activeCellCount, 0.0f);
    std::fill(cells.angularAcc_y.begin(), cells.angularAcc_y.begin() + cells.activeCellCount, 0.0f);
    std::fill(cells.angularAcc_z.begin(), cells.angularAcc_z.begin() + cells.activeCellCount, 0.0f);
    
    // Process each active cell's adhesion connections
    for (size_t cellIndex = 0; cellIndex < cells.activeCellCount; ++cellIndex) {
        // Iterate through the cell's 20 adhesion slots (matching GPU approach)
        for (int slotIndex = 0; slotIndex < 20; ++slotIndex) {
            int connectionIndex = cells.adhesionIndices[cellIndex][slotIndex];
            
            // Skip empty slots (-1 indicates no connection)
            if (connectionIndex < 0 || connectionIndex >= static_cast<int>(adhesions.activeConnectionCount)) {
                continue;
            }
            
            // Verify connection is active
            if (adhesions.isActive[connectionIndex] == 0) {
                continue;
            }
            
            // Get the two cells involved in this connection
            uint32_t cellA = adhesions.cellAIndex[connectionIndex];
            uint32_t cellB = adhesions.cellBIndex[connectionIndex];
            
            // Only process if this cell is cellA (to avoid double processing)
            // GPU approach: each cell processes connections where it is cellA
            if (cellA != cellIndex) {
                continue;
            }
            
            // Validate cell indices
            if (cellB >= cells.activeCellCount) {
                continue;
            }
            
            // Get mode settings for this connection
            uint32_t modeIndex = adhesions.modeIndex[connectionIndex];
            if (modeIndex >= modeSettings.size()) {
                continue; // Skip connections with invalid mode indices
            }
            
            const GPUModeAdhesionSettings& settings = modeSettings[modeIndex];
            
            // The adhesion calculator will handle the conversion internally
            
            // Calculate forces and torques using complete GPU-equivalent algorithm
            glm::vec3 forceA(0.0f), torqueA(0.0f);
            glm::vec3 forceB(0.0f), torqueB(0.0f);
            
            // Create a temporary single-connection structure for the adhesion calculator
            CPUAdhesionConnections_SoA tempConnections;
            tempConnections.activeConnectionCount = 1;
            
            // Copy the connection data to the temporary structure
            tempConnections.cellAIndex[0] = adhesions.cellAIndex[connectionIndex];
            tempConnections.cellBIndex[0] = adhesions.cellBIndex[connectionIndex];
            tempConnections.modeIndex[0] = adhesions.modeIndex[connectionIndex];
            tempConnections.isActive[0] = adhesions.isActive[connectionIndex];
            tempConnections.zoneA[0] = adhesions.zoneA[connectionIndex];
            tempConnections.zoneB[0] = adhesions.zoneB[connectionIndex];
            
            tempConnections.anchorDirectionA_x[0] = adhesions.anchorDirectionA_x[connectionIndex];
            tempConnections.anchorDirectionA_y[0] = adhesions.anchorDirectionA_y[connectionIndex];
            tempConnections.anchorDirectionA_z[0] = adhesions.anchorDirectionA_z[connectionIndex];
            tempConnections.anchorDirectionB_x[0] = adhesions.anchorDirectionB_x[connectionIndex];
            tempConnections.anchorDirectionB_y[0] = adhesions.anchorDirectionB_y[connectionIndex];
            tempConnections.anchorDirectionB_z[0] = adhesions.anchorDirectionB_z[connectionIndex];
            
            tempConnections.twistReferenceA_x[0] = adhesions.twistReferenceA_x[connectionIndex];
            tempConnections.twistReferenceA_y[0] = adhesions.twistReferenceA_y[connectionIndex];
            tempConnections.twistReferenceA_z[0] = adhesions.twistReferenceA_z[connectionIndex];
            tempConnections.twistReferenceA_w[0] = adhesions.twistReferenceA_w[connectionIndex];
            tempConnections.twistReferenceB_x[0] = adhesions.twistReferenceB_x[connectionIndex];
            tempConnections.twistReferenceB_y[0] = adhesions.twistReferenceB_y[connectionIndex];
            tempConnections.twistReferenceB_z[0] = adhesions.twistReferenceB_z[connectionIndex];
            tempConnections.twistReferenceB_w[0] = adhesions.twistReferenceB_w[connectionIndex];
            
            // Create a temporary cells structure with just the two cells involved
            CPUCellPhysics_SoA tempCells;
            tempCells.activeCellCount = std::max(cellA, cellB) + 1;
            
            // Copy the relevant cell data (this is inefficient but maintains interface)
            for (size_t i = 0; i <= std::max(cellA, cellB); ++i) {
                if (i < cells.activeCellCount) {
                    tempCells.pos_x[i] = cells.pos_x[i];
                    tempCells.pos_y[i] = cells.pos_y[i];
                    tempCells.pos_z[i] = cells.pos_z[i];
                    tempCells.vel_x[i] = cells.vel_x[i];
                    tempCells.vel_y[i] = cells.vel_y[i];
                    tempCells.vel_z[i] = cells.vel_z[i];
                    tempCells.acc_x[i] = cells.acc_x[i];
                    tempCells.acc_y[i] = cells.acc_y[i];
                    tempCells.acc_z[i] = cells.acc_z[i];
                    tempCells.quat_x[i] = cells.quat_x[i];
                    tempCells.quat_y[i] = cells.quat_y[i];
                    tempCells.quat_z[i] = cells.quat_z[i];
                    tempCells.quat_w[i] = cells.quat_w[i];
                    tempCells.angularVel_x[i] = cells.angularVel_x[i];
                    tempCells.angularVel_y[i] = cells.angularVel_y[i];
                    tempCells.angularVel_z[i] = cells.angularVel_z[i];
                    tempCells.angularAcc_x[i] = cells.angularAcc_x[i];
                    tempCells.angularAcc_y[i] = cells.angularAcc_y[i];
                    tempCells.angularAcc_z[i] = cells.angularAcc_z[i];
                    tempCells.mass[i] = cells.mass[i];
                    tempCells.radius[i] = cells.radius[i];
                    tempCells.age[i] = cells.age[i];
                    tempCells.energy[i] = cells.energy[i];
                }
            }
            
            // Store original accelerations to calculate the force difference
            float origAccA_x = tempCells.acc_x[cellA];
            float origAccA_y = tempCells.acc_y[cellA];
            float origAccA_z = tempCells.acc_z[cellA];
            float origAccB_x = tempCells.acc_x[cellB];
            float origAccB_y = tempCells.acc_y[cellB];
            float origAccB_z = tempCells.acc_z[cellB];
            
            float origAngAccA_x = tempCells.angularAcc_x[cellA];
            float origAngAccA_y = tempCells.angularAcc_y[cellA];
            float origAngAccA_z = tempCells.angularAcc_z[cellA];
            float origAngAccB_x = tempCells.angularAcc_x[cellB];
            float origAngAccB_y = tempCells.angularAcc_y[cellB];
            float origAngAccB_z = tempCells.angularAcc_z[cellB];
            
            // Use the complete adhesion force calculator (GPU-equivalent)
            m_adhesionCalculator->computeAdhesionForces(tempConnections, tempCells, modeSettings, 0.0f);
            
            // Calculate the force difference and apply to original cells
            float deltaAccA_x = tempCells.acc_x[cellA] - origAccA_x;
            float deltaAccA_y = tempCells.acc_y[cellA] - origAccA_y;
            float deltaAccA_z = tempCells.acc_z[cellA] - origAccA_z;
            float deltaAccB_x = tempCells.acc_x[cellB] - origAccB_x;
            float deltaAccB_y = tempCells.acc_y[cellB] - origAccB_y;
            float deltaAccB_z = tempCells.acc_z[cellB] - origAccB_z;
            
            float deltaAngAccA_x = tempCells.angularAcc_x[cellA] - origAngAccA_x;
            float deltaAngAccA_y = tempCells.angularAcc_y[cellA] - origAngAccA_y;
            float deltaAngAccA_z = tempCells.angularAcc_z[cellA] - origAngAccA_z;
            float deltaAngAccB_x = tempCells.angularAcc_x[cellB] - origAngAccB_x;
            float deltaAngAccB_y = tempCells.angularAcc_y[cellB] - origAngAccB_y;
            float deltaAngAccB_z = tempCells.angularAcc_z[cellB] - origAngAccB_z;
            
            // Apply the calculated accelerations to the original cells
            cells.acc_x[cellA] += deltaAccA_x;
            cells.acc_y[cellA] += deltaAccA_y;
            cells.acc_z[cellA] += deltaAccA_z;
            cells.acc_x[cellB] += deltaAccB_x;
            cells.acc_y[cellB] += deltaAccB_y;
            cells.acc_z[cellB] += deltaAccB_z;
            
            cells.angularAcc_x[cellA] += deltaAngAccA_x;
            cells.angularAcc_y[cellA] += deltaAngAccA_y;
            cells.angularAcc_z[cellA] += deltaAngAccA_z;
            cells.angularAcc_x[cellB] += deltaAngAccB_x;
            cells.angularAcc_y[cellB] += deltaAngAccB_y;
            cells.angularAcc_z[cellB] += deltaAngAccB_z;
        }
    }
}

void CPUSIMDPhysicsEngine::integrateVerlet(CPUCellPhysics_SoA& cells, float deltaTime) {
    // Behavioral equivalence with GPU: cell_position_update.comp Verlet integration
    const float dt2 = deltaTime * deltaTime;
    const float half_dt2 = 0.5f * dt2;
    
    // SIMD-optimized Verlet integration matching GPU algorithm exactly
    const __m256 dt_vec = _mm256_set1_ps(deltaTime);
    const __m256 half_dt2_vec = _mm256_set1_ps(half_dt2);
    
    size_t simd_count = (cells.activeCellCount / SIMD_WIDTH) * SIMD_WIDTH;
    
    // Process cells in SIMD batches
    for (size_t i = 0; i < simd_count; i += SIMD_WIDTH) {
        // Load current positions, velocities, and accelerations
        __m256 pos_x = _mm256_load_ps(&cells.pos_x[i]);
        __m256 pos_y = _mm256_load_ps(&cells.pos_y[i]);
        __m256 pos_z = _mm256_load_ps(&cells.pos_z[i]);
        
        __m256 vel_x = _mm256_load_ps(&cells.vel_x[i]);
        __m256 vel_y = _mm256_load_ps(&cells.vel_y[i]);
        __m256 vel_z = _mm256_load_ps(&cells.vel_z[i]);
        
        __m256 acc_x = _mm256_load_ps(&cells.acc_x[i]);
        __m256 acc_y = _mm256_load_ps(&cells.acc_y[i]);
        __m256 acc_z = _mm256_load_ps(&cells.acc_z[i]);
        
        __m256 age = _mm256_load_ps(&cells.age[i]);
        
        // GPU algorithm: pos += vel * deltaTime + 0.5 * acc * (deltaTime * deltaTime)
        __m256 vel_dt_x = _mm256_mul_ps(vel_x, dt_vec);
        __m256 vel_dt_y = _mm256_mul_ps(vel_y, dt_vec);
        __m256 vel_dt_z = _mm256_mul_ps(vel_z, dt_vec);
        
        __m256 acc_half_dt2_x = _mm256_mul_ps(acc_x, half_dt2_vec);
        __m256 acc_half_dt2_y = _mm256_mul_ps(acc_y, half_dt2_vec);
        __m256 acc_half_dt2_z = _mm256_mul_ps(acc_z, half_dt2_vec);
        
        __m256 new_pos_x = _mm256_add_ps(_mm256_add_ps(pos_x, vel_dt_x), acc_half_dt2_x);
        __m256 new_pos_y = _mm256_add_ps(_mm256_add_ps(pos_y, vel_dt_y), acc_half_dt2_y);
        __m256 new_pos_z = _mm256_add_ps(_mm256_add_ps(pos_z, vel_dt_z), acc_half_dt2_z);
        
        // GPU algorithm: No velocity update in position update shader
        // (Velocity is updated in separate velocity update shader)
        
        // GPU algorithm: Total age increment per frame is deltaTime (0.5 in position + 0.5 in velocity)
        // Since CPU does both in one step, increment by full deltaTime
        __m256 dt = _mm256_set1_ps(deltaTime);
        __m256 new_age = _mm256_add_ps(age, dt);
        
        // Store results back
        _mm256_store_ps(&cells.pos_x[i], new_pos_x);
        _mm256_store_ps(&cells.pos_y[i], new_pos_y);
        _mm256_store_ps(&cells.pos_z[i], new_pos_z);
        _mm256_store_ps(&cells.age[i], new_age);
        
        // Reset acceleration for next frame (GPU behavior)
        __m256 zero = _mm256_setzero_ps();
        _mm256_store_ps(&cells.acc_x[i], zero);
        _mm256_store_ps(&cells.acc_y[i], zero);
        _mm256_store_ps(&cells.acc_z[i], zero);
        
        // Reset angular acceleration for next frame
        _mm256_store_ps(&cells.angularAcc_x[i], zero);
        _mm256_store_ps(&cells.angularAcc_y[i], zero);
        _mm256_store_ps(&cells.angularAcc_z[i], zero);
    }
    
    // Handle remaining cells with scalar operations (GPU algorithm)
    for (size_t i = simd_count; i < cells.activeCellCount; ++i) {
        // GPU Verlet integration: pos += vel * deltaTime + 0.5 * acc * (deltaTime * deltaTime)
        cells.pos_x[i] += cells.vel_x[i] * deltaTime + 0.5f * cells.acc_x[i] * dt2;
        cells.pos_y[i] += cells.vel_y[i] * deltaTime + 0.5f * cells.acc_y[i] * dt2;
        cells.pos_z[i] += cells.vel_z[i] * deltaTime + 0.5f * cells.acc_z[i] * dt2;
        
        // GPU algorithm: Total age increment per frame is deltaTime (0.5 in position + 0.5 in velocity)
        // Since CPU does both in one step, increment by full deltaTime
        cells.age[i] += deltaTime;
        
        // Reset acceleration for next frame (GPU behavior)
        cells.acc_x[i] = 0.0f;
        cells.acc_y[i] = 0.0f;
        cells.acc_z[i] = 0.0f;
        
        // Reset angular acceleration for next frame
        cells.angularAcc_x[i] = 0.0f;
        cells.angularAcc_y[i] = 0.0f;
        cells.angularAcc_z[i] = 0.0f;
    }
}

void CPUSIMDPhysicsEngine::updateOrientations(CPUCellPhysics_SoA& cells, float deltaTime) {
    // Update orientation based on angular velocity (GPU-equivalent)
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        glm::vec3 angularVelocity(cells.angularVel_x[i], cells.angularVel_y[i], cells.angularVel_z[i]);
        float angularSpeed = glm::length(angularVelocity);
        
        if (angularSpeed > 0.001f) {
            // Convert angular velocity to quaternion rotation
            glm::vec3 axis = angularVelocity / angularSpeed;
            float angle = angularSpeed * deltaTime;
            
            // Create rotation quaternion from axis-angle
            float halfAngle = angle * 0.5f;
            float sinHalfAngle = std::sin(halfAngle);
            glm::quat rotation(std::cos(halfAngle), axis * sinHalfAngle);
            
            // Current orientation
            glm::quat currentOrientation(cells.quat_w[i], cells.quat_x[i], cells.quat_y[i], cells.quat_z[i]);
            
            // Apply rotation: newOrientation = rotation * currentOrientation
            glm::quat newOrientation = rotation * currentOrientation;
            newOrientation = glm::normalize(newOrientation);
            
            // Store updated orientation
            cells.quat_w[i] = newOrientation.w;
            cells.quat_x[i] = newOrientation.x;
            cells.quat_y[i] = newOrientation.y;
            cells.quat_z[i] = newOrientation.z;
        }
    }
}

void CPUSIMDPhysicsEngine::updateSpatialGrid(const CPUCellPhysics_SoA& cells) {
    // Always use spatial grid for collision optimization (even in preview mode)
    if (!m_spatialGrid) {
        return;
    }
    
    // Only skip for very small cell counts where direct collision is faster (2 or fewer)
    if (cells.activeCellCount <= 2) {
        return;
    }
    
    m_spatialGrid->clear();
    
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        glm::vec3 position(cells.pos_x[i], cells.pos_y[i], cells.pos_z[i]);
        m_spatialGrid->insert(static_cast<uint32_t>(i), position);
    }
}

std::vector<uint32_t> CPUSIMDPhysicsEngine::getNeighbors(uint32_t cellIndex, float radius) {
    if (cellIndex >= 256) {
        return {};
    }
    
    // This would use the spatial grid to find neighbors efficiently
    // For now, return empty vector as placeholder
    return {};
}

void CPUSIMDPhysicsEngine::applyAccelerationDamping(CPUCellPhysics_SoA& cells) {
    // Behavioral equivalence with GPU: cell_physics_spatial.comp acceleration damping
    const float accelerationDamping = 0.5f; // GPU uniform u_accelerationDamping
    
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        glm::vec3 acceleration(cells.acc_x[i], cells.acc_y[i], cells.acc_z[i]);
        float accMagnitude = glm::length(acceleration);
        
        // Multi-tier acceleration damping to prevent drift (GPU algorithm)
        if (accMagnitude < 0.001f) {
            // Eliminate extremely tiny forces entirely
            acceleration = glm::vec3(0.0f);
        } else if (accMagnitude < 0.01f) {
            // Very aggressive damping for very small forces
            acceleration *= 0.1f;
        } else if (accMagnitude < 0.05f) {
            // Strong damping for small forces
            acceleration *= accelerationDamping;
        }
        
        // Store damped acceleration
        cells.acc_x[i] = acceleration.x;
        cells.acc_y[i] = acceleration.y;
        cells.acc_z[i] = acceleration.z;
    }
}

void CPUSIMDPhysicsEngine::processCellBatch(CPUCellPhysics_SoA& cells, size_t startIdx, size_t count) {
    const size_t endIdx = std::min(startIdx + count, cells.activeCellCount);
    
    // Use direct collision detection only for very small cell counts (2 or fewer)
    if (!m_spatialGrid || cells.activeCellCount <= 2) {
        for (size_t i = startIdx; i < endIdx; ++i) {
            for (size_t j = i + 1; j < cells.activeCellCount; ++j) {
                processCollisionPair(static_cast<uint32_t>(i), static_cast<uint32_t>(j), cells);
            }
        }
        return;
    }
    
    // Use spatial grid for 3+ cells (reduces O(n²) to O(n) for typical cases)
    prefetchCellData(cells, startIdx, endIdx - startIdx);
    
    for (size_t i = startIdx; i < endIdx; ++i) {
        glm::vec3 position(cells.pos_x[i], cells.pos_y[i], cells.pos_z[i]);
        float radius = cells.radius[i];
        
        m_spatialGrid->iterateNeighbors(position, radius * 2.0f, 
            [this, &cells, i](uint32_t j) {
                if (j <= i || j >= cells.activeCellCount) return;
                processCollisionPair(static_cast<uint32_t>(i), j, cells);
            });
    }
}

void CPUSIMDPhysicsEngine::prefetchCellData(const CPUCellPhysics_SoA& cells, size_t startIdx, size_t count) {
    // Prefetch cell data into CPU cache for better performance
    const size_t endIdx = std::min(startIdx + count, cells.activeCellCount);
    
    // Prefetch position data
    for (size_t i = startIdx; i < endIdx; i += 8) { // Cache line size optimization
        _mm_prefetch(reinterpret_cast<const char*>(&cells.pos_x[i]), _MM_HINT_T0);
        _mm_prefetch(reinterpret_cast<const char*>(&cells.pos_y[i]), _MM_HINT_T0);
        _mm_prefetch(reinterpret_cast<const char*>(&cells.pos_z[i]), _MM_HINT_T0);
        _mm_prefetch(reinterpret_cast<const char*>(&cells.radius[i]), _MM_HINT_T0);
        _mm_prefetch(reinterpret_cast<const char*>(&cells.mass[i]), _MM_HINT_T0);
    }
}

void CPUSIMDPhysicsEngine::processCollisionPair(uint32_t cellA, uint32_t cellB, CPUCellPhysics_SoA& cells) {
    // Behavioral equivalence with GPU: cell_physics_spatial.comp collision detection

    // OPTIMIZATION: Use pre-calculated radius from SoA data instead of recalculating pow(mass, 1/3)
    float myRadius = cells.radius[cellA];
    float otherRadius = cells.radius[cellB];

    // Early rejection based on radius - skip if cells are too far apart
    float maxInteractionDist = myRadius + otherRadius + 0.5f;

    // Calculate distance squared first (cheaper than full distance)
    float dx = cells.pos_x[cellA] - cells.pos_x[cellB];
    float dy = cells.pos_y[cellA] - cells.pos_y[cellB];
    float dz = cells.pos_z[cellA] - cells.pos_z[cellB];
    float distSq = dx * dx + dy * dy + dz * dz;

    // Early distance check using squared distance (GPU optimization)
    float maxDistSq = maxInteractionDist * maxInteractionDist;
    if (distSq > maxDistSq || distSq < 0.000001f) {
        return;
    }

    // Now calculate actual distance only if needed
    float distance = std::sqrt(distSq);
    float minDistance = myRadius + otherRadius;

    if (distance < minDistance) {
        // Collision detected - apply repulsion force (identical to GPU)
        float invDistance = 1.0f / distance;
        glm::vec3 direction(dx * invDistance, dy * invDistance, dz * invDistance);

        float overlap = minDistance - distance;
        float hardness = 10.0f; // GPU constant
        glm::vec3 totalForce = direction * overlap * hardness;

        // Calculate acceleration (F = ma, so a = F/m) - GPU algorithm
        float myMass = cells.mass[cellA];
        float otherMass = cells.mass[cellB];
        glm::vec3 acceleration = totalForce / myMass;

        // Apply acceleration to cell A
        cells.acc_x[cellA] += acceleration.x;
        cells.acc_y[cellA] += acceleration.y;
        cells.acc_z[cellA] += acceleration.z;

        // Newton's third law: equal and opposite force on cell B
        glm::vec3 otherAcceleration = -totalForce / otherMass;
        cells.acc_x[cellB] += otherAcceleration.x;
        cells.acc_y[cellB] += otherAcceleration.y;
        cells.acc_z[cellB] += otherAcceleration.z;
    }
}

void CPUSIMDPhysicsEngine::processBatchCollisions(CPUCellPhysics_SoA& cells) {
    // Process cells in SIMD-friendly batches for dense collision regions
    const size_t batchSize = SIMD_WIDTH;
    
    for (size_t start = 0; start < cells.activeCellCount; start += batchSize) {
        size_t count = std::min(batchSize, cells.activeCellCount - start);
        
        // Use SIMD batch collision detection for this group
        simd_collision_detection_batch(
            cells.pos_x.data(), cells.pos_y.data(), cells.pos_z.data(),
            cells.radius.data(), cells.acc_x.data(), cells.acc_y.data(), cells.acc_z.data(),
            cells.mass.data(), start, count
        );
    }
}

void CPUSIMDPhysicsEngine::simd_collision_detection_batch(const float* pos_x, const float* pos_y, const float* pos_z,
                                                         const float* radius, float* acc_x, float* acc_y, float* acc_z,
                                                         const float* mass, size_t start_idx, size_t count) {
    // SIMD-optimized collision detection for a batch of cells
    // This processes multiple collision tests simultaneously using AVX2
    
    const __m256 spring_constant = _mm256_set1_ps(1000.0f);
    const __m256 epsilon = _mm256_set1_ps(0.001f);
    
    // Process each cell in the batch against all other cells in the batch
    for (size_t i = 0; i < count; ++i) {
        size_t cell_i = start_idx + i;
        
        // Load cell i data
        __m256 pos_i_x = _mm256_set1_ps(pos_x[cell_i]);
        __m256 pos_i_y = _mm256_set1_ps(pos_y[cell_i]);
        __m256 pos_i_z = _mm256_set1_ps(pos_z[cell_i]);
        __m256 radius_i = _mm256_set1_ps(radius[cell_i]);
        __m256 mass_i = _mm256_set1_ps(mass[cell_i]);
        
        // Process against up to 8 other cells simultaneously
        for (size_t j = i + 1; j < count && j < i + SIMD_WIDTH; ++j) {
            size_t cell_j = start_idx + j;
            
            // Calculate distance and collision for multiple pairs
            float dx = pos_x[cell_j] - pos_x[cell_i];
            float dy = pos_y[cell_j] - pos_y[cell_i];
            float dz = pos_z[cell_j] - pos_z[cell_i];
            
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            float min_distance = radius[cell_i] + radius[cell_j];
            
            if (distance < min_distance && distance > 0.001f) {
                float penetration = min_distance - distance;
                float force_magnitude = penetration * 1000.0f;
                
                // Normalize collision normal
                float inv_distance = 1.0f / distance;
                float normal_x = dx * inv_distance;
                float normal_y = dy * inv_distance;
                float normal_z = dz * inv_distance;
                
                // Calculate forces
                float force_x = normal_x * force_magnitude;
                float force_y = normal_y * force_magnitude;
                float force_z = normal_z * force_magnitude;
                
                // Apply forces (Newton's third law)
                if (mass[cell_i] > 0.0f) {
                    acc_x[cell_i] += force_x / mass[cell_i];
                    acc_y[cell_i] += force_y / mass[cell_i];
                    acc_z[cell_i] += force_z / mass[cell_i];
                }
                
                if (mass[cell_j] > 0.0f) {
                    acc_x[cell_j] -= force_x / mass[cell_j];
                    acc_y[cell_j] -= force_y / mass[cell_j];
                    acc_z[cell_j] -= force_z / mass[cell_j];
                }
            }
        }
    }
}

bool CPUSIMDPhysicsEngine::sphereCollisionTest(const glm::vec3& posA, float radiusA,
                                              const glm::vec3& posB, float radiusB,
                                              float& penetrationDepth, glm::vec3& normal) {
    glm::vec3 delta = posB - posA;
    float distance = glm::length(delta);
    float minDistance = radiusA + radiusB;
    
    if (distance < minDistance && distance > 0.001f) {
        penetrationDepth = minDistance - distance;
        normal = delta / distance;
        return true;
    }
    
    return false;
}

void CPUSIMDPhysicsEngine::processAdhesionConnection(uint32_t connectionIndex,
                                                    const CPUAdhesionConnections_SoA& adhesions,
                                                    CPUCellPhysics_SoA& cells,
                                                    const std::vector<GPUModeAdhesionSettings>& modeSettings) {
    // Use the complete adhesion force calculator with full torque support
    // This ensures behavioral equivalence with GPU adhesion_physics.comp
    
    uint32_t cellA = adhesions.cellAIndex[connectionIndex];
    uint32_t cellB = adhesions.cellBIndex[connectionIndex];
    uint32_t modeIndex = adhesions.modeIndex[connectionIndex];
    
    if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount) {
        return; // Invalid connection
    }
    
    if (modeIndex >= modeSettings.size()) {
        return; // Invalid mode index
    }
    
    // Create a temporary single-connection structure for the adhesion calculator
    CPUAdhesionConnections_SoA tempConnections;
    tempConnections.activeConnectionCount = 1;
    
    // Copy the connection data
    tempConnections.cellAIndex[0] = adhesions.cellAIndex[connectionIndex];
    tempConnections.cellBIndex[0] = adhesions.cellBIndex[connectionIndex];
    tempConnections.modeIndex[0] = adhesions.modeIndex[connectionIndex];
    tempConnections.isActive[0] = adhesions.isActive[connectionIndex];
    tempConnections.zoneA[0] = adhesions.zoneA[connectionIndex];
    tempConnections.zoneB[0] = adhesions.zoneB[connectionIndex];
    
    tempConnections.anchorDirectionA_x[0] = adhesions.anchorDirectionA_x[connectionIndex];
    tempConnections.anchorDirectionA_y[0] = adhesions.anchorDirectionA_y[connectionIndex];
    tempConnections.anchorDirectionA_z[0] = adhesions.anchorDirectionA_z[connectionIndex];
    tempConnections.anchorDirectionB_x[0] = adhesions.anchorDirectionB_x[connectionIndex];
    tempConnections.anchorDirectionB_y[0] = adhesions.anchorDirectionB_y[connectionIndex];
    tempConnections.anchorDirectionB_z[0] = adhesions.anchorDirectionB_z[connectionIndex];
    
    tempConnections.twistReferenceA_x[0] = adhesions.twistReferenceA_x[connectionIndex];
    tempConnections.twistReferenceA_y[0] = adhesions.twistReferenceA_y[connectionIndex];
    tempConnections.twistReferenceA_z[0] = adhesions.twistReferenceA_z[connectionIndex];
    tempConnections.twistReferenceA_w[0] = adhesions.twistReferenceA_w[connectionIndex];
    tempConnections.twistReferenceB_x[0] = adhesions.twistReferenceB_x[connectionIndex];
    tempConnections.twistReferenceB_y[0] = adhesions.twistReferenceB_y[connectionIndex];
    tempConnections.twistReferenceB_z[0] = adhesions.twistReferenceB_z[connectionIndex];
    tempConnections.twistReferenceB_w[0] = adhesions.twistReferenceB_w[connectionIndex];
    
    // Use the complete adhesion force calculator (GPU-equivalent with full torque support)
    // This properly calculates both linear forces and rotational torques
    m_adhesionCalculator->computeAdhesionForces(tempConnections, cells, modeSettings, 0.0f);
}

void CPUSIMDPhysicsEngine::simd_adhesion_force_batch(CPUCellPhysics_SoA& cells,
                                                    const CPUAdhesionConnections_SoA& adhesions,
                                                    const std::vector<GPUModeAdhesionSettings>& modeSettings,
                                                    size_t start_idx, size_t count) {
    // SIMD-optimized adhesion force calculation for a batch of connections
    // Process multiple adhesion connections simultaneously using AVX2
    
    const __m256 epsilon = _mm256_set1_ps(0.001f);
    
    // Process connections in the batch
    for (size_t i = 0; i < count; ++i) {
        size_t conn_idx = start_idx + i;
        
        uint32_t cellA = adhesions.cellAIndex[conn_idx];
        uint32_t cellB = adhesions.cellBIndex[conn_idx];
        uint32_t modeIndex = adhesions.modeIndex[conn_idx];
        
        if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount) {
            continue; // Invalid connection
        }
        
        if (modeIndex >= modeSettings.size()) {
            continue; // Invalid mode index
        }
        
        // Get mode-specific adhesion settings (Requirements 4.1, 4.2)
        const GPUModeAdhesionSettings& settings = modeSettings[modeIndex];
        
        // Load positions and properties
        float posA_x = cells.pos_x[cellA];
        float posA_y = cells.pos_y[cellA];
        float posA_z = cells.pos_z[cellA];
        
        float posB_x = cells.pos_x[cellB];
        float posB_y = cells.pos_y[cellB];
        float posB_z = cells.pos_z[cellB];
        
        // Use mode-specific parameters (Requirements 4.3, 4.4)
        float restLength = settings.restLength;
        float stiffness = settings.linearSpringStiffness;
        float dampingCoeff = settings.linearSpringDamping;
        
        // Calculate delta vector
        float delta_x = posB_x - posA_x;
        float delta_y = posB_y - posA_y;
        float delta_z = posB_z - posA_z;
        
        // Calculate current length
        float currentLength = std::sqrt(delta_x * delta_x + delta_y * delta_y + delta_z * delta_z);
        
        if (currentLength > 0.001f) {
            // Calculate direction (normalized delta)
            float inv_length = 1.0f / currentLength;
            float dir_x = delta_x * inv_length;
            float dir_y = delta_y * inv_length;
            float dir_z = delta_z * inv_length;
            
            // Calculate spring force magnitude
            float extension = currentLength - restLength;
            float springForceMagnitude = extension * stiffness;
            
            // Calculate damping force (oppose relative motion)
            float velA_x = cells.vel_x[cellA];
            float velA_y = cells.vel_y[cellA];
            float velA_z = cells.vel_z[cellA];
            float velB_x = cells.vel_x[cellB];
            float velB_y = cells.vel_y[cellB];
            float velB_z = cells.vel_z[cellB];
            
            float relVel_x = velB_x - velA_x;
            float relVel_y = velB_y - velA_y;
            float relVel_z = velB_z - velA_z;
            
            float relVelDotDir = relVel_x * dir_x + relVel_y * dir_y + relVel_z * dir_z;
            float dampingMagnitude = 1.0f - dampingCoeff * relVelDotDir;
            
            // Total force magnitude
            float totalForceMagnitude = springForceMagnitude + dampingMagnitude;
            
            // Calculate force vector
            float force_x = dir_x * totalForceMagnitude;
            float force_y = dir_y * totalForceMagnitude;
            float force_z = dir_z * totalForceMagnitude;
            
            // Apply forces (Newton's third law)
            float massA = cells.mass[cellA];
            float massB = cells.mass[cellB];
            
            if (massA > 0.0f) {
                cells.acc_x[cellA] += force_x / massA;
                cells.acc_y[cellA] += force_y / massA;
                cells.acc_z[cellA] += force_z / massA;
            }
            
            if (massB > 0.0f) {
                cells.acc_x[cellB] -= force_x / massB;
                cells.acc_y[cellB] -= force_y / massB;
                cells.acc_z[cellB] -= force_z / massB;
            }
        }
    }
}

void CPUSIMDPhysicsEngine::updateVelocities(CPUCellPhysics_SoA& cells, float deltaTime) {
    // Behavioral equivalence with GPU: cell_velocity_update.comp
    // Uses Verlet velocity integration with damping (CRITICAL for stability)
    
    const float damping = 0.98f; // GPU uniform u_damping
    const float dampingFactor = std::pow(damping, deltaTime * 100.0f);
    
    // Scalar implementation for clarity and correctness
    // (SIMD optimization can be added later once behavior is verified)
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        // --- Linear Verlet velocity (GPU algorithm) ---
        glm::vec3 vel(cells.vel_x[i], cells.vel_y[i], cells.vel_z[i]);
        glm::vec3 acc_old(cells.prevAcc_x[i], cells.prevAcc_y[i], cells.prevAcc_z[i]);
        glm::vec3 acc_new(cells.acc_x[i], cells.acc_y[i], cells.acc_z[i]);
        
        // Verlet integration: vel += 0.5 * (acc_old + acc_new) * deltaTime
        vel += 0.5f * (acc_old + acc_new) * deltaTime;
        
        // Apply damping: vel *= pow(damping, deltaTime * 100.0)
        vel *= dampingFactor;
        
        cells.vel_x[i] = vel.x;
        cells.vel_y[i] = vel.y;
        cells.vel_z[i] = vel.z;
        
        // Store current acceleration as previous for next frame
        cells.prevAcc_x[i] = acc_new.x;
        cells.prevAcc_y[i] = acc_new.y;
        cells.prevAcc_z[i] = acc_new.z;
        
        // --- Angular Verlet velocity (GPU algorithm) ---
        glm::vec3 angVel(cells.angularVel_x[i], cells.angularVel_y[i], cells.angularVel_z[i]);
        glm::vec3 angAcc_old(cells.prevAngularAcc_x[i], cells.prevAngularAcc_y[i], cells.prevAngularAcc_z[i]);
        glm::vec3 angAcc_new(cells.angularAcc_x[i], cells.angularAcc_y[i], cells.angularAcc_z[i]);
        
        // Verlet integration: angularVel += 0.5 * (angAcc_old + angAcc_new) * deltaTime
        angVel += 0.5f * (angAcc_old + angAcc_new) * deltaTime;
        
        // Apply damping: angularVel *= pow(damping, deltaTime * 100.0)
        angVel *= dampingFactor;
        
        cells.angularVel_x[i] = angVel.x;
        cells.angularVel_y[i] = angVel.y;
        cells.angularVel_z[i] = angVel.z;
        
        // Store current angular acceleration as previous for next frame
        cells.prevAngularAcc_x[i] = angAcc_new.x;
        cells.prevAngularAcc_y[i] = angAcc_new.y;
        cells.prevAngularAcc_z[i] = angAcc_new.z;
    }
}

void CPUSIMDPhysicsEngine::applyBoundaryConstraints(CPUCellPhysics_SoA& cells) {
    const float worldSize = 100.0f; // config::WORLD_SIZE
    const float halfWorld = worldSize * 0.5f;
    
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        // Apply spherical boundary constraints
        glm::vec3 position(cells.pos_x[i], cells.pos_y[i], cells.pos_z[i]);
        float distance = glm::length(position);
        float radius = cells.radius[i];
        
        if (distance + radius > halfWorld) {
            // Push cell back inside boundary
            glm::vec3 direction = position / distance;
            glm::vec3 newPosition = direction * (halfWorld - radius);
            
            cells.pos_x[i] = newPosition.x;
            cells.pos_y[i] = newPosition.y;
            cells.pos_z[i] = newPosition.z;
            
            // Reverse velocity component pointing outward
            glm::vec3 velocity(cells.vel_x[i], cells.vel_y[i], cells.vel_z[i]);
            float velocityDotDirection = glm::dot(velocity, direction);
            
            if (velocityDotDirection > 0.0f) {
                glm::vec3 reflectedVelocity = velocity - 2.0f * velocityDotDirection * direction;
                reflectedVelocity *= 0.8f; // Apply damping
                
                cells.vel_x[i] = reflectedVelocity.x;
                cells.vel_y[i] = reflectedVelocity.y;
                cells.vel_z[i] = reflectedVelocity.z;
            }
        }
    }
}

// CPU SIMD utility functions (AVX2 optimized implementations)
void CPUSIMDPhysicsEngine::simd_vec3_add(float* a_x, float* a_y, float* a_z,
                                         const float* b_x, const float* b_y, const float* b_z,
                                         size_t count) {
    // Process 8 floats at a time using AVX2
    size_t simd_count = (count / SIMD_WIDTH) * SIMD_WIDTH;
    
    for (size_t i = 0; i < simd_count; i += SIMD_WIDTH) {
        // Load 8 floats from each array
        __m256 ax = _mm256_load_ps(&a_x[i]);
        __m256 ay = _mm256_load_ps(&a_y[i]);
        __m256 az = _mm256_load_ps(&a_z[i]);
        
        __m256 bx = _mm256_load_ps(&b_x[i]);
        __m256 by = _mm256_load_ps(&b_y[i]);
        __m256 bz = _mm256_load_ps(&b_z[i]);
        
        // Perform vectorized addition
        __m256 result_x = _mm256_add_ps(ax, bx);
        __m256 result_y = _mm256_add_ps(ay, by);
        __m256 result_z = _mm256_add_ps(az, bz);
        
        // Store results back
        _mm256_store_ps(&a_x[i], result_x);
        _mm256_store_ps(&a_y[i], result_y);
        _mm256_store_ps(&a_z[i], result_z);
    }
    
    // Handle remaining elements with scalar operations
    for (size_t i = simd_count; i < count; ++i) {
        a_x[i] += b_x[i];
        a_y[i] += b_y[i];
        a_z[i] += b_z[i];
    }
}

void CPUSIMDPhysicsEngine::simd_vec3_scale(float* vec_x, float* vec_y, float* vec_z,
                                          float scalar, size_t count) {
    // Broadcast scalar to all 8 elements of AVX2 register
    __m256 scalar_vec = _mm256_set1_ps(scalar);
    
    // Process 8 floats at a time using AVX2
    size_t simd_count = (count / SIMD_WIDTH) * SIMD_WIDTH;
    
    for (size_t i = 0; i < simd_count; i += SIMD_WIDTH) {
        // Load 8 floats from each component
        __m256 vx = _mm256_load_ps(&vec_x[i]);
        __m256 vy = _mm256_load_ps(&vec_y[i]);
        __m256 vz = _mm256_load_ps(&vec_z[i]);
        
        // Perform vectorized multiplication
        __m256 result_x = _mm256_mul_ps(vx, scalar_vec);
        __m256 result_y = _mm256_mul_ps(vy, scalar_vec);
        __m256 result_z = _mm256_mul_ps(vz, scalar_vec);
        
        // Store results back
        _mm256_store_ps(&vec_x[i], result_x);
        _mm256_store_ps(&vec_y[i], result_y);
        _mm256_store_ps(&vec_z[i], result_z);
    }
    
    // Handle remaining elements with scalar operations
    for (size_t i = simd_count; i < count; ++i) {
        vec_x[i] *= scalar;
        vec_y[i] *= scalar;
        vec_z[i] *= scalar;
    }
}

void CPUSIMDPhysicsEngine::simd_vec3_normalize(float* vec_x, float* vec_y, float* vec_z, size_t count) {
    const __m256 epsilon = _mm256_set1_ps(0.001f);
    const __m256 one = _mm256_set1_ps(1.0f);
    
    // Process 8 floats at a time using AVX2
    size_t simd_count = (count / SIMD_WIDTH) * SIMD_WIDTH;
    
    for (size_t i = 0; i < simd_count; i += SIMD_WIDTH) {
        // Load 8 vectors
        __m256 vx = _mm256_load_ps(&vec_x[i]);
        __m256 vy = _mm256_load_ps(&vec_y[i]);
        __m256 vz = _mm256_load_ps(&vec_z[i]);
        
        // Calculate length squared: x² + y² + z²
        __m256 x_sq = _mm256_mul_ps(vx, vx);
        __m256 y_sq = _mm256_mul_ps(vy, vy);
        __m256 z_sq = _mm256_mul_ps(vz, vz);
        __m256 length_sq = _mm256_add_ps(_mm256_add_ps(x_sq, y_sq), z_sq);
        
        // Calculate length using fast reciprocal square root approximation
        __m256 length = _mm256_sqrt_ps(length_sq);
        
        // Create mask for valid lengths (> epsilon)
        __m256 valid_mask = _mm256_cmp_ps(length, epsilon, _CMP_GT_OQ);
        
        // Calculate reciprocal length (1/length) for normalization
        __m256 inv_length = _mm256_div_ps(one, length);
        
        // Apply normalization only where length > epsilon
        __m256 norm_x = _mm256_mul_ps(vx, inv_length);
        __m256 norm_y = _mm256_mul_ps(vy, inv_length);
        __m256 norm_z = _mm256_mul_ps(vz, inv_length);
        
        // Blend normalized and original values based on mask
        __m256 result_x = _mm256_blendv_ps(vx, norm_x, valid_mask);
        __m256 result_y = _mm256_blendv_ps(vy, norm_y, valid_mask);
        __m256 result_z = _mm256_blendv_ps(vz, norm_z, valid_mask);
        
        // Store results back
        _mm256_store_ps(&vec_x[i], result_x);
        _mm256_store_ps(&vec_y[i], result_y);
        _mm256_store_ps(&vec_z[i], result_z);
    }
    
    // Handle remaining elements with scalar operations
    for (size_t i = simd_count; i < count; ++i) {
        float length = std::sqrt(vec_x[i] * vec_x[i] + vec_y[i] * vec_y[i] + vec_z[i] * vec_z[i]);
        if (length > 0.001f) {
            vec_x[i] /= length;
            vec_y[i] /= length;
            vec_z[i] /= length;
        }
    }
}

void CPUSIMDPhysicsEngine::simd_distance_squared(const float* pos1_x, const float* pos1_y, const float* pos1_z,
                                                const float* pos2_x, const float* pos2_y, const float* pos2_z,
                                                float* result, size_t count) {
    // Process 8 distance calculations simultaneously using AVX2
    size_t simd_count = (count / SIMD_WIDTH) * SIMD_WIDTH;
    
    for (size_t i = 0; i < simd_count; i += SIMD_WIDTH) {
        // Load 8 positions from each array
        __m256 p1x = _mm256_load_ps(&pos1_x[i]);
        __m256 p1y = _mm256_load_ps(&pos1_y[i]);
        __m256 p1z = _mm256_load_ps(&pos1_z[i]);
        
        __m256 p2x = _mm256_load_ps(&pos2_x[i]);
        __m256 p2y = _mm256_load_ps(&pos2_y[i]);
        __m256 p2z = _mm256_load_ps(&pos2_z[i]);
        
        // Calculate delta vectors
        __m256 dx = _mm256_sub_ps(p2x, p1x);
        __m256 dy = _mm256_sub_ps(p2y, p1y);
        __m256 dz = _mm256_sub_ps(p2z, p1z);
        
        // Calculate squared distances: dx² + dy² + dz²
        __m256 dx_sq = _mm256_mul_ps(dx, dx);
        __m256 dy_sq = _mm256_mul_ps(dy, dy);
        __m256 dz_sq = _mm256_mul_ps(dz, dz);
        __m256 dist_sq = _mm256_add_ps(_mm256_add_ps(dx_sq, dy_sq), dz_sq);
        
        // Store results
        _mm256_store_ps(&result[i], dist_sq);
    }
    
    // Handle remaining elements with scalar operations
    for (size_t i = simd_count; i < count; ++i) {
        float dx = pos2_x[i] - pos1_x[i];
        float dy = pos2_y[i] - pos1_y[i];
        float dz = pos2_z[i] - pos1_z[i];
        result[i] = dx * dx + dy * dy + dz * dz;
    }
}

// CPU Spatial Grid implementation (cache-optimized)
void CPUSIMDPhysicsEngine::CPUSpatialGrid::initialize() {
    gridCells.resize(TOTAL_GRID_CELLS);
    queryResultBuffer.reserve(256); // Preallocate for typical queries
    neighborCoords.reserve(27); // 3x3x3 neighborhood
    
    // Precompute neighbor offsets for cache efficiency
    precomputeNeighborOffsets(1); // Search radius of 1
}

void CPUSIMDPhysicsEngine::CPUSpatialGrid::clear() {
    // Fast clear using SIMD-friendly operations
    for (auto& cell : gridCells) {
        cell.count = 0;
        // No need to clear the array contents, just reset count
    }
}

void CPUSIMDPhysicsEngine::CPUSpatialGrid::insert(uint32_t cellIndex, const glm::vec3& position) {
    glm::ivec3 coord = getGridCoord(position);
    if (isValidCoord(coord)) {
        uint32_t gridIndex = getGridIndex(coord);
        GridCell& cell = gridCells[gridIndex];
        
        // Add cell if there's space (avoid dynamic allocation)
        if (cell.count < MAX_CELLS_PER_GRID) {
            cell.cellIndices[cell.count] = cellIndex;
            cell.count++;
        }
        // If grid cell is full, we skip this cell (graceful degradation)
        // This maintains performance at the cost of some accuracy in dense regions
    }
}

std::vector<uint32_t> CPUSIMDPhysicsEngine::CPUSpatialGrid::query(const glm::vec3& position, float radius) {
    queryResultBuffer.clear();
    queryIntoBuffer(position, radius, queryResultBuffer);
    return queryResultBuffer;
}

void CPUSIMDPhysicsEngine::CPUSpatialGrid::queryIntoBuffer(const glm::vec3& position, float radius, 
                                                          std::vector<uint32_t>& results) {
    results.clear();
    
    glm::ivec3 centerCoord = getGridCoord(position);
    int searchRadius = static_cast<int>(std::ceil(radius / CELL_SIZE));
    
    // Use precomputed neighbor offsets for cache efficiency
    for (const auto& offset : neighborOffsets) {
        glm::ivec3 coord = centerCoord + offset * searchRadius;
        
        if (isValidCoord(coord)) {
            uint32_t gridIndex = getGridIndex(coord);
            const GridCell& cell = gridCells[gridIndex];
            
            // SIMD-friendly loop over cell contents
            for (uint32_t i = 0; i < cell.count; ++i) {
                results.push_back(cell.cellIndices[i]);
            }
        }
    }
}



glm::ivec3 CPUSIMDPhysicsEngine::CPUSpatialGrid::getGridCoord(const glm::vec3& position) {
    const float worldSize = 100.0f;
    const float halfWorld = worldSize * 0.5f;
    
    // Optimized coordinate calculation
    glm::vec3 normalized = (position + glm::vec3(halfWorld)) / worldSize;
    glm::ivec3 coord = glm::ivec3(normalized * static_cast<float>(GRID_SIZE));
    
    // Clamp to valid range (branchless where possible)
    coord.x = std::max(0, std::min(GRID_SIZE - 1, coord.x));
    coord.y = std::max(0, std::min(GRID_SIZE - 1, coord.y));
    coord.z = std::max(0, std::min(GRID_SIZE - 1, coord.z));
    
    return coord;
}

bool CPUSIMDPhysicsEngine::CPUSpatialGrid::isValidCoord(const glm::ivec3& coord) {
    return coord.x >= 0 && coord.x < GRID_SIZE &&
           coord.y >= 0 && coord.y < GRID_SIZE &&
           coord.z >= 0 && coord.z < GRID_SIZE;
}

uint32_t CPUSIMDPhysicsEngine::CPUSpatialGrid::getGridIndex(const glm::ivec3& coord) {
    // Optimized 3D to 1D index conversion
    return static_cast<uint32_t>(coord.x + coord.y * GRID_SIZE + coord.z * GRID_SIZE * GRID_SIZE);
}

void CPUSIMDPhysicsEngine::CPUSpatialGrid::precomputeNeighborOffsets(int searchRadius) {
    neighborOffsets.clear();
    
    // Precompute all neighbor offsets for cache-friendly access
    for (int dz = -searchRadius; dz <= searchRadius; ++dz) {
        for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
            for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                neighborOffsets.emplace_back(dx, dy, dz);
            }
        }
    }
}
void CPUSIMDPhysicsEngine::checkCellDivision(CPUCellPhysics_SoA& cells, 
                                             CPUAdhesionConnections_SoA& adhesions, 
                                             float deltaTime,
                                             const CPUGenomeParameters* genomeParams) {
    // Get division threshold from genome parameters
    float divisionThreshold = 2.0f; // Default division threshold
    if (genomeParams) {
        divisionThreshold = genomeParams->divisionThreshold;
    }
    
    // Check each cell for division based on age and division threshold
    std::vector<uint32_t> cellsToSplit;
    
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        float cellAge = cells.age[i];
        
        if (cellAge >= divisionThreshold) {
            cellsToSplit.push_back(static_cast<uint32_t>(i));
        }
    }
    
    // Process cell divisions (matching GPU capacity check)
    for (uint32_t cellIndex : cellsToSplit) {
        if (cells.activeCellCount >= 256) { // CPU limit is 256 cells
            break; // No space available, cancel remaining splits
        }
        
        // Initialize genome orientation if not already set (first division)
        // Check if parent's genome orientation is uninitialized (all zeros or invalid)
        if (cells.genomeQuat_w[cellIndex] == 0.0f && cells.genomeQuat_x[cellIndex] == 0.0f &&
            cells.genomeQuat_y[cellIndex] == 0.0f && cells.genomeQuat_z[cellIndex] == 0.0f) {
            // Initialize parent's genome orientation to identity quaternion
            cells.genomeQuat_w[cellIndex] = 1.0f;
            cells.genomeQuat_x[cellIndex] = 0.0f;
            cells.genomeQuat_y[cellIndex] = 0.0f;
            cells.genomeQuat_z[cellIndex] = 0.0f;
        }
        
        // Create daughter cell
        uint32_t daughterIndex = static_cast<uint32_t>(cells.activeCellCount);
        cells.activeCellCount++;
        
        // Copy parent cell properties to daughter
        cells.pos_x[daughterIndex] = cells.pos_x[cellIndex];
        cells.pos_y[daughterIndex] = cells.pos_y[cellIndex];
        cells.pos_z[daughterIndex] = cells.pos_z[cellIndex];
        
        cells.vel_x[daughterIndex] = cells.vel_x[cellIndex];
        cells.vel_y[daughterIndex] = cells.vel_y[cellIndex];
        cells.vel_z[daughterIndex] = cells.vel_z[cellIndex];
        
        cells.acc_x[daughterIndex] = 0.0f;
        cells.acc_y[daughterIndex] = 0.0f;
        cells.acc_z[daughterIndex] = 0.0f;
        
        // Apply child orientations from genome parameters (matching GPU implementation)
        // Get parent orientation
        glm::quat parentOrientation(cells.quat_w[cellIndex], cells.quat_x[cellIndex], 
                                     cells.quat_y[cellIndex], cells.quat_z[cellIndex]);
        
        // Get child orientation deltas from genome parameters
        glm::quat childOrientationA = genomeParams ? genomeParams->childOrientationA : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::quat childOrientationB = genomeParams ? genomeParams->childOrientationB : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        
        // Apply child orientations: newOrientation = parentOrientation * childOrientation (matching GPU)
        glm::quat newOrientationA = glm::normalize(parentOrientation * childOrientationA);
        glm::quat newOrientationB = glm::normalize(parentOrientation * childOrientationB);
        
        // Apply child A orientation to parent cell (reusing parent index)
        cells.quat_w[cellIndex] = newOrientationA.w;
        cells.quat_x[cellIndex] = newOrientationA.x;
        cells.quat_y[cellIndex] = newOrientationA.y;
        cells.quat_z[cellIndex] = newOrientationA.z;
        
        // Apply child B orientation to daughter cell
        cells.quat_w[daughterIndex] = newOrientationB.w;
        cells.quat_x[daughterIndex] = newOrientationB.x;
        cells.quat_y[daughterIndex] = newOrientationB.y;
        cells.quat_z[daughterIndex] = newOrientationB.z;
        
        // GPU behavior: Mass is NOT split - both children keep parent's mass
        float originalMass = cells.mass[cellIndex];
        cells.mass[cellIndex] = originalMass;
        cells.mass[daughterIndex] = originalMass;
        
        // Radius is calculated as cube root of mass (GPU formula: pow(mass, 1.0/3.0))
        float radius = std::pow(originalMass, 1.0f/3.0f);
        cells.radius[cellIndex] = radius;
        cells.radius[daughterIndex] = radius;
        
        // GPU behavior: Age is reset to excess beyond split interval
        float cellAge = cells.age[cellIndex];
        float startAge = cellAge - divisionThreshold;
        cells.age[cellIndex] = startAge;
        cells.age[daughterIndex] = startAge + 0.001f; // Slight offset like GPU
        
        // Copy other properties
        cells.energy[cellIndex] = cells.energy[cellIndex] * 0.5f; // Split energy
        cells.energy[daughterIndex] = cells.energy[cellIndex];
        
        cells.cellType[daughterIndex] = cells.cellType[cellIndex];
        cells.genomeID[daughterIndex] = cells.genomeID[cellIndex];
        cells.flags[daughterIndex] = cells.flags[cellIndex];
        
        cells.color_r[daughterIndex] = cells.color_r[cellIndex];
        cells.color_g[daughterIndex] = cells.color_g[cellIndex];
        cells.color_b[daughterIndex] = cells.color_b[cellIndex];
        
        // GPU behavior: Use split direction from genome
        glm::vec3 splitDirection = glm::vec3(1.0f, 0.0f, 0.0f); // Default direction
        if (genomeParams) {
            splitDirection = genomeParams->splitDirection;
            if (glm::length(splitDirection) < 0.001f) {
                splitDirection = glm::vec3(1.0f, 0.0f, 0.0f); // Fallback
            } else {
                splitDirection = glm::normalize(splitDirection);
            }
        }
        
        // GPU behavior: Rotate split direction by parent's genome orientation (matching GPU shader)
        // GPU does: rotateVectorByQuaternion(mode.splitDirection.xyz, cell.genomeOrientation)
        glm::vec3 worldSplitDirection = parentOrientation * splitDirection;
        
        // GPU behavior: Move cells apart by 0.5 units in rotated split direction
        glm::vec3 offset = worldSplitDirection * 0.5f;
        
        // Child A gets +offset, Child B gets -offset (matching GPU)
        cells.pos_x[cellIndex] += offset.x;      // Child A (parent index)
        cells.pos_y[cellIndex] += offset.y;
        cells.pos_z[cellIndex] += offset.z;
        
        cells.pos_x[daughterIndex] -= offset.x;  // Child B (new index)
        cells.pos_y[daughterIndex] -= offset.y;
        cells.pos_z[daughterIndex] -= offset.z;
        
        // GPU behavior: No velocity separation - cells inherit parent velocity
        
        // Implement complete division inheritance system (Requirements 8.1-8.5, 9.1-9.5, 10.1-10.5)
        // Only create adhesion connections if adhesion is enabled in the genome (check bit 8 for adhesion capability)
        bool adhesionEnabled = genomeParams && (genomeParams->cellTypeFlags & (1 << 8)) != 0;
        if (m_divisionInheritanceHandler && adhesionEnabled) {
            // Get child mode settings for inheritance flags from genome (bit 0 = childA, bit 1 = childB)
            bool childAKeepAdhesion = genomeParams ? ((genomeParams->divisionFlags & (1 << 0)) != 0) : true;
            bool childBKeepAdhesion = genomeParams ? ((genomeParams->divisionFlags & (1 << 1)) != 0) : true;

            // Get genome orientations for anchor direction calculations from genome parameters
            glm::quat orientationA = genomeParams ? genomeParams->childOrientationA : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            glm::quat orientationB = genomeParams ? genomeParams->childOrientationB : glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            
            // GPU behavior: Save parent's genome orientation BEFORE updating (needed for inheritance)
            // GPU uses cell.genomeOrientation which is the parent's orientation before division
            glm::quat parentGenomeOrientationBeforeDivision(
                cells.genomeQuat_w[cellIndex],
                cells.genomeQuat_x[cellIndex],
                cells.genomeQuat_y[cellIndex],
                cells.genomeQuat_z[cellIndex]
            );
            
            // GPU behavior: Update genome orientations for child cells
            // GPU does: childA.genomeOrientation = normalize(quatMultiply(parent.genomeOrientation, mode.orientationA))
            glm::quat childAGenomeOrientation = glm::normalize(parentGenomeOrientationBeforeDivision * orientationA);
            glm::quat childBGenomeOrientation = glm::normalize(parentGenomeOrientationBeforeDivision * orientationB);
            
            // CRITICAL FIX: Use splitDirection directly from genome, NOT perpendicular plane
            // The inheritance handler expects splitDirection for zone classification
            
            // Create complete GPUMode from genome parameters
            GPUMode parentMode{};

            // Set split direction from genome (CRITICAL for correct anchor placement and zone classification)
            parentMode.splitDirection = glm::vec4(splitDirection, 0.0f);

            // Set orientation deltas for children from genome parameters
            parentMode.orientationA = orientationA;
            parentMode.orientationB = orientationB;

            // Set adhesion inheritance flags
            parentMode.parentMakeAdhesion = 1; // Enable child-to-child adhesion
            parentMode.childAKeepAdhesion = childAKeepAdhesion ? 1 : 0;
            parentMode.childBKeepAdhesion = childBKeepAdhesion ? 1 : 0;
            parentMode.childModes = glm::ivec2(0, 0); // Both children use same mode (mode 0)

            // Use actual adhesion settings from genome
            const AdhesionSettings& adhesion = genomeParams->adhesionSettings;
            parentMode.adhesionSettings.canBreak = adhesion.canBreak ? 1 : 0;
            parentMode.adhesionSettings.breakForce = adhesion.breakForce;
            parentMode.adhesionSettings.restLength = adhesion.restLength;
            parentMode.adhesionSettings.linearSpringStiffness = adhesion.linearSpringStiffness;
            parentMode.adhesionSettings.linearSpringDamping = adhesion.linearSpringDamping;
            parentMode.adhesionSettings.orientationSpringStiffness = adhesion.orientationSpringStiffness;
            parentMode.adhesionSettings.orientationSpringDamping = adhesion.orientationSpringDamping;
            parentMode.adhesionSettings.maxAngularDeviation = adhesion.maxAngularDeviation;
            parentMode.adhesionSettings.twistConstraintStiffness = adhesion.twistConstraintStiffness;
            parentMode.adhesionSettings.twistConstraintDamping = adhesion.twistConstraintDamping;
            parentMode.adhesionSettings.enableTwistConstraint = adhesion.enableTwistConstraint ? 1 : 0;

            // Create modes array (just one mode for now - single-mode genome)
            std::vector<GPUMode> allModes;
            allModes.push_back(parentMode);

            // Perform complete adhesion inheritance with geometric anchor placement
            // CRITICAL: Inheritance must happen BEFORE updating genome orientations
            // GPU uses parent's genome orientation (before division) for inheritance calculations
            m_divisionInheritanceHandler->inheritAdhesionsOnDivision(
                cellIndex,           // Parent cell index
                cellIndex,           // Child A index (reuses parent index)
                daughterIndex,       // Child B index (new cell)
                splitDirection,      // FIXED: Split direction from genome (NOT perpendicular plane)
                offset,              // Split offset vector
                orientationA,        // Child A genome orientation
                orientationB,        // Child B genome orientation
                childAKeepAdhesion,  // Child A inheritance flag
                childBKeepAdhesion,  // Child B inheritance flag
                cells,               // Cell physics data
                adhesions,           // Adhesion connections data
                parentMode,          // Parent mode data
                allModes             // All mode data
            );
            
            // NOW update genome orientations for child cells (AFTER inheritance)
            // This matches GPU behavior: inheritance uses parent's orientation, then cells are updated
            cells.genomeQuat_w[cellIndex] = childAGenomeOrientation.w;
            cells.genomeQuat_x[cellIndex] = childAGenomeOrientation.x;
            cells.genomeQuat_y[cellIndex] = childAGenomeOrientation.y;
            cells.genomeQuat_z[cellIndex] = childAGenomeOrientation.z;
            
            cells.genomeQuat_w[daughterIndex] = childBGenomeOrientation.w;
            cells.genomeQuat_x[daughterIndex] = childBGenomeOrientation.x;
            cells.genomeQuat_y[daughterIndex] = childBGenomeOrientation.y;
            cells.genomeQuat_z[daughterIndex] = childBGenomeOrientation.z;
        }
    }
}

// SIMD-Optimized Batch Processing Implementation (Requirements 5.1-5.5)

void CPUSIMDPhysicsEngine::SIMDAdhesionBatchProcessor::processAllConnections(
    CPUCellPhysics_SoA& cells,
    const CPUAdhesionConnections_SoA& adhesions,
    const std::vector<GPUModeAdhesionSettings>& modeSettings) {
    
    // Process connections in batches of 8 using AVX2 (Requirement 5.1, 5.2)
    const size_t totalBatches = (adhesions.activeConnectionCount + SIMD_WIDTH - 1) / SIMD_WIDTH;
    const size_t maxBatches = std::min(totalBatches, BATCH_COUNT); // 640 batches max
    
    // Process full SIMD batches
    for (size_t batchIndex = 0; batchIndex < maxBatches; ++batchIndex) {
        size_t startConnection = batchIndex * SIMD_WIDTH;
        size_t endConnection = std::min(startConnection + SIMD_WIDTH, adhesions.activeConnectionCount);
        
        // Skip if this batch would be empty
        if (startConnection >= adhesions.activeConnectionCount) {
            break;
        }
        
        // Process this batch of up to 8 connections
        processBatch(cells, adhesions, modeSettings, batchIndex);
    }
}

void CPUSIMDPhysicsEngine::SIMDAdhesionBatchProcessor::processBatch(
    CPUCellPhysics_SoA& cells,
    const CPUAdhesionConnections_SoA& adhesions,
    const std::vector<GPUModeAdhesionSettings>& modeSettings,
    size_t batchIndex) {
    
    // Cache-optimized data gathering (Requirement 5.5)
    gatherBatchData(cells, adhesions, modeSettings, batchIndex);
    
    // SIMD force calculation for 8 connections simultaneously (Requirement 5.4)
    calculateSIMDForces();
    
    // Apply calculated forces back to cells
    scatterForces(cells);
}

void CPUSIMDPhysicsEngine::SIMDAdhesionBatchProcessor::gatherBatchData(
    const CPUCellPhysics_SoA& cells,
    const CPUAdhesionConnections_SoA& adhesions,
    const std::vector<GPUModeAdhesionSettings>& modeSettings,
    size_t batchIndex) {
    
    const size_t startConnection = batchIndex * SIMD_WIDTH;
    const size_t endConnection = std::min(startConnection + SIMD_WIDTH, adhesions.activeConnectionCount);
    
    // Gather connection data for this batch (cache-friendly sequential access)
    for (size_t i = 0; i < SIMD_WIDTH; ++i) {
        size_t connectionIndex = startConnection + i;
        
        if (connectionIndex < endConnection && connectionIndex < adhesions.activeConnectionCount) {
            // Skip inactive connections
            if (adhesions.isActive[connectionIndex] == 0) {
                // Fill with dummy data that will produce zero forces
                buffers.cellA_indices[i] = 0;
                buffers.cellB_indices[i] = 0;
                buffers.mode_indices[i] = 0;
                buffers.temp_posA_x[i] = 0.0f;
                buffers.temp_posA_y[i] = 0.0f;
                buffers.temp_posA_z[i] = 0.0f;
                buffers.temp_posB_x[i] = 0.0f;
                buffers.temp_posB_y[i] = 0.0f;
                buffers.temp_posB_z[i] = 0.0f;
                buffers.rest_length[i] = 0.0f;
                buffers.stiffness[i] = 0.0f;
                buffers.damping[i] = 0.0f;
                continue;
            }
            
            uint32_t cellA = adhesions.cellAIndex[connectionIndex];
            uint32_t cellB = adhesions.cellBIndex[connectionIndex];
            uint32_t modeIndex = adhesions.modeIndex[connectionIndex];
            
            // Validate indices
            if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount || 
                modeIndex >= modeSettings.size()) {
                // Fill with dummy data for invalid connections
                buffers.cellA_indices[i] = 0;
                buffers.cellB_indices[i] = 0;
                buffers.mode_indices[i] = 0;
                buffers.temp_posA_x[i] = 0.0f;
                buffers.temp_posA_y[i] = 0.0f;
                buffers.temp_posA_z[i] = 0.0f;
                buffers.temp_posB_x[i] = 0.0f;
                buffers.temp_posB_y[i] = 0.0f;
                buffers.temp_posB_z[i] = 0.0f;
                buffers.rest_length[i] = 0.0f;
                buffers.stiffness[i] = 0.0f;
                buffers.damping[i] = 0.0f;
                continue;
            }
            
            // Store cell indices
            buffers.cellA_indices[i] = cellA;
            buffers.cellB_indices[i] = cellB;
            buffers.mode_indices[i] = modeIndex;
            
            // Gather cell positions (cache-friendly access)
            buffers.temp_posA_x[i] = cells.pos_x[cellA];
            buffers.temp_posA_y[i] = cells.pos_y[cellA];
            buffers.temp_posA_z[i] = cells.pos_z[cellA];
            buffers.temp_posB_x[i] = cells.pos_x[cellB];
            buffers.temp_posB_y[i] = cells.pos_y[cellB];
            buffers.temp_posB_z[i] = cells.pos_z[cellB];
            
            // Gather cell velocities
            buffers.temp_velA_x[i] = cells.vel_x[cellA];
            buffers.temp_velA_y[i] = cells.vel_y[cellA];
            buffers.temp_velA_z[i] = cells.vel_z[cellA];
            buffers.temp_velB_x[i] = cells.vel_x[cellB];
            buffers.temp_velB_y[i] = cells.vel_y[cellB];
            buffers.temp_velB_z[i] = cells.vel_z[cellB];
            
            // Gather cell masses
            buffers.temp_massA[i] = cells.mass[cellA];
            buffers.temp_massB[i] = cells.mass[cellB];
            
            // Gather anchor directions
            buffers.temp_anchorA_x[i] = adhesions.anchorDirectionA_x[connectionIndex];
            buffers.temp_anchorA_y[i] = adhesions.anchorDirectionA_y[connectionIndex];
            buffers.temp_anchorA_z[i] = adhesions.anchorDirectionA_z[connectionIndex];
            buffers.temp_anchorB_x[i] = adhesions.anchorDirectionB_x[connectionIndex];
            buffers.temp_anchorB_y[i] = adhesions.anchorDirectionB_y[connectionIndex];
            buffers.temp_anchorB_z[i] = adhesions.anchorDirectionB_z[connectionIndex];
            
            // Gather mode settings
            const GPUModeAdhesionSettings& settings = modeSettings[modeIndex];
            buffers.rest_length[i] = settings.restLength;
            buffers.stiffness[i] = settings.linearSpringStiffness;
            buffers.damping[i] = settings.linearSpringDamping;
        } else {
            // Fill remaining slots with dummy data
            buffers.cellA_indices[i] = 0;
            buffers.cellB_indices[i] = 0;
            buffers.mode_indices[i] = 0;
            buffers.temp_posA_x[i] = 0.0f;
            buffers.temp_posA_y[i] = 0.0f;
            buffers.temp_posA_z[i] = 0.0f;
            buffers.temp_posB_x[i] = 0.0f;
            buffers.temp_posB_y[i] = 0.0f;
            buffers.temp_posB_z[i] = 0.0f;
            buffers.rest_length[i] = 0.0f;
            buffers.stiffness[i] = 0.0f;
            buffers.damping[i] = 0.0f;
        }
    }
}

void CPUSIMDPhysicsEngine::SIMDAdhesionBatchProcessor::calculateSIMDForces() {
    // Load position data into AVX2 registers
    __m256 posA_x = _mm256_load_ps(buffers.temp_posA_x.data());
    __m256 posA_y = _mm256_load_ps(buffers.temp_posA_y.data());
    __m256 posA_z = _mm256_load_ps(buffers.temp_posA_z.data());
    __m256 posB_x = _mm256_load_ps(buffers.temp_posB_x.data());
    __m256 posB_y = _mm256_load_ps(buffers.temp_posB_y.data());
    __m256 posB_z = _mm256_load_ps(buffers.temp_posB_z.data());
    
    // Load velocity data
    __m256 velA_x = _mm256_load_ps(buffers.temp_velA_x.data());
    __m256 velA_y = _mm256_load_ps(buffers.temp_velA_y.data());
    __m256 velA_z = _mm256_load_ps(buffers.temp_velA_z.data());
    __m256 velB_x = _mm256_load_ps(buffers.temp_velB_x.data());
    __m256 velB_y = _mm256_load_ps(buffers.temp_velB_y.data());
    __m256 velB_z = _mm256_load_ps(buffers.temp_velB_z.data());
    
    // Load mode settings
    __m256 rest_length = _mm256_load_ps(buffers.rest_length.data());
    __m256 stiffness = _mm256_load_ps(buffers.stiffness.data());
    __m256 damping = _mm256_load_ps(buffers.damping.data());
    
    // Calculate delta vectors (B - A)
    __m256 delta_x = _mm256_sub_ps(posB_x, posA_x);
    __m256 delta_y = _mm256_sub_ps(posB_y, posA_y);
    __m256 delta_z = _mm256_sub_ps(posB_z, posA_z);
    
    // Store delta vectors for later use
    _mm256_store_ps(buffers.delta_x.data(), delta_x);
    _mm256_store_ps(buffers.delta_y.data(), delta_y);
    _mm256_store_ps(buffers.delta_z.data(), delta_z);
    
    // Calculate distance squared: dx² + dy² + dz²
    __m256 dx_sq = _mm256_mul_ps(delta_x, delta_x);
    __m256 dy_sq = _mm256_mul_ps(delta_y, delta_y);
    __m256 dz_sq = _mm256_mul_ps(delta_z, delta_z);
    __m256 dist_sq = _mm256_add_ps(_mm256_add_ps(dx_sq, dy_sq), dz_sq);
    
    // Calculate distance using square root
    __m256 distance = _mm256_sqrt_ps(dist_sq);
    _mm256_store_ps(buffers.distance.data(), distance);
    
    // Calculate inverse distance for normalization (with epsilon protection)
    const __m256 epsilon = _mm256_set1_ps(0.0001f);
    const __m256 one = _mm256_set1_ps(1.0f);
    __m256 safe_distance = _mm256_max_ps(distance, epsilon);
    __m256 inv_distance = _mm256_div_ps(one, safe_distance);
    _mm256_store_ps(buffers.inv_distance.data(), inv_distance);
    
    // Calculate adhesion direction (normalized delta)
    __m256 adhesion_dir_x = _mm256_mul_ps(delta_x, inv_distance);
    __m256 adhesion_dir_y = _mm256_mul_ps(delta_y, inv_distance);
    __m256 adhesion_dir_z = _mm256_mul_ps(delta_z, inv_distance);
    
    // Calculate spring force magnitude: stiffness * (distance - rest_length)
    __m256 extension = _mm256_sub_ps(distance, rest_length);
    __m256 spring_force_mag = _mm256_mul_ps(stiffness, extension);
    
    // Calculate relative velocity (B - A)
    __m256 rel_vel_x = _mm256_sub_ps(velB_x, velA_x);
    __m256 rel_vel_y = _mm256_sub_ps(velB_y, velA_y);
    __m256 rel_vel_z = _mm256_sub_ps(velB_z, velA_z);
    
    // Calculate damping: 1.0 - damping * dot(rel_vel, adhesion_dir) (GPU algorithm)
    __m256 rel_vel_dot_dir = _mm256_add_ps(
        _mm256_add_ps(
            _mm256_mul_ps(rel_vel_x, adhesion_dir_x),
            _mm256_mul_ps(rel_vel_y, adhesion_dir_y)
        ),
        _mm256_mul_ps(rel_vel_z, adhesion_dir_z)
    );

    // GPU algorithm: dampMag = 1.0 - damping * dot(relVel, adhesionDir)
    __m256 damping_mag = _mm256_sub_ps(one, _mm256_mul_ps(damping, rel_vel_dot_dir));

    // GPU algorithm: dampingForce = -adhesionDir * dampMag
    // Since we're calculating magnitude only, this becomes -dampMag
    __m256 damping_force_mag = _mm256_sub_ps(_mm256_setzero_ps(), damping_mag);

    // Total force magnitude (spring + damping force)
    __m256 total_force_mag = _mm256_add_ps(spring_force_mag, damping_force_mag);
    
    // Calculate force vectors
    __m256 force_x = _mm256_mul_ps(adhesion_dir_x, total_force_mag);
    __m256 force_y = _mm256_mul_ps(adhesion_dir_y, total_force_mag);
    __m256 force_z = _mm256_mul_ps(adhesion_dir_z, total_force_mag);
    
    // Store calculated forces
    _mm256_store_ps(buffers.force_x.data(), force_x);
    _mm256_store_ps(buffers.force_y.data(), force_y);
    _mm256_store_ps(buffers.force_z.data(), force_z);
}

void CPUSIMDPhysicsEngine::SIMDAdhesionBatchProcessor::scatterForces(CPUCellPhysics_SoA& cells) {
    // Apply calculated forces to cells (Newton's third law: equal and opposite forces)
    for (size_t i = 0; i < SIMD_WIDTH; ++i) {
        uint32_t cellA = buffers.cellA_indices[i];
        uint32_t cellB = buffers.cellB_indices[i];
        
        // Skip if this is dummy data or invalid indices
        if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount) {
            continue;
        }
        
        float force_x = buffers.force_x[i];
        float force_y = buffers.force_y[i];
        float force_z = buffers.force_z[i];
        
        float massA = buffers.temp_massA[i];
        float massB = buffers.temp_massB[i];
        
        // Apply force to cell A (F = ma, so a = F/m)
        if (massA > 0.0f) {
            cells.acc_x[cellA] += force_x / massA;
            cells.acc_y[cellA] += force_y / massA;
            cells.acc_z[cellA] += force_z / massA;
        }
        
        // Apply opposite force to cell B (Newton's third law)
        if (massB > 0.0f) {
            cells.acc_x[cellB] -= force_x / massB;
            cells.acc_y[cellB] -= force_y / massB;
            cells.acc_z[cellB] -= force_z / massB;
        }
    }
}

bool CPUSIMDPhysicsEngine::SIMDAdhesionBatchProcessor::validateSIMDPrecision(
    const CPUCellPhysics_SoA& cells,
    const CPUAdhesionConnections_SoA& adhesions,
    const std::vector<GPUModeAdhesionSettings>& modeSettings,
    size_t batchIndex) {
    
    // Validation: Compare SIMD results with scalar calculations (Requirement 5.4)
    const float tolerance = 1e-6f; // Numerical precision tolerance
    const size_t startConnection = batchIndex * SIMD_WIDTH;
    
    for (size_t i = 0; i < SIMD_WIDTH; ++i) {
        size_t connectionIndex = startConnection + i;
        
        if (connectionIndex >= adhesions.activeConnectionCount || 
            adhesions.isActive[connectionIndex] == 0) {
            continue;
        }
        
        uint32_t cellA = adhesions.cellAIndex[connectionIndex];
        uint32_t cellB = adhesions.cellBIndex[connectionIndex];
        uint32_t modeIndex = adhesions.modeIndex[connectionIndex];
        
        if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount || 
            modeIndex >= modeSettings.size()) {
            continue;
        }
        
        // Calculate scalar reference values
        glm::vec3 posA(cells.pos_x[cellA], cells.pos_y[cellA], cells.pos_z[cellA]);
        glm::vec3 posB(cells.pos_x[cellB], cells.pos_y[cellB], cells.pos_z[cellB]);
        glm::vec3 velA(cells.vel_x[cellA], cells.vel_y[cellA], cells.vel_z[cellA]);
        glm::vec3 velB(cells.vel_x[cellB], cells.vel_y[cellB], cells.vel_z[cellB]);
        
        glm::vec3 delta = posB - posA;
        float distance = glm::length(delta);
        
        if (distance < 0.0001f) continue;
        
        glm::vec3 adhesionDir = delta / distance;
        const GPUModeAdhesionSettings& settings = modeSettings[modeIndex];
        
        // Scalar force calculation
        float springForceMag = settings.linearSpringStiffness * (distance - settings.restLength);
        glm::vec3 relVel = velB - velA;
        float dampingMag = settings.linearSpringDamping * glm::dot(relVel, adhesionDir);
        float totalForceMag = springForceMag + dampingMag;
        glm::vec3 scalarForce = adhesionDir * totalForceMag;
        
        // Compare with SIMD results
        glm::vec3 simdForce(buffers.force_x[i], buffers.force_y[i], buffers.force_z[i]);
        glm::vec3 error = scalarForce - simdForce;
        float errorMagnitude = glm::length(error);
        
        if (errorMagnitude > tolerance) {
            // SIMD precision validation failed
            return false;
        }
    }
    
    return true; // All validations passed
}

// Testing and Validation Methods for SIMD Batch Processing

void CPUSIMDPhysicsEngine::testSIMDBatchProcessor() {
    if (!m_simdBatchProcessor) {
        std::cerr << "SIMD Batch Processor not initialized!" << std::endl;
        return;
    }
    
    std::cout << "=== SIMD Batch Processor Test ===" << std::endl;
    
    // Create test data
    CPUCellPhysics_SoA testCells;
    CPUAdhesionConnections_SoA testAdhesions;
    std::vector<GPUModeAdhesionSettings> testModeSettings;
    
    // Initialize test cells
    testCells.activeCellCount = 16; // Test with 16 cells
    for (size_t i = 0; i < testCells.activeCellCount; ++i) {
        testCells.pos_x[i] = static_cast<float>(i) * 2.0f;
        testCells.pos_y[i] = 0.0f;
        testCells.pos_z[i] = 0.0f;
        testCells.vel_x[i] = 0.0f;
        testCells.vel_y[i] = 0.0f;
        testCells.vel_z[i] = 0.0f;
        testCells.acc_x[i] = 0.0f;
        testCells.acc_y[i] = 0.0f;
        testCells.acc_z[i] = 0.0f;
        testCells.mass[i] = 1.0f;
        testCells.radius[i] = 0.5f;
    }
    
    // Initialize test adhesion connections
    testAdhesions.activeConnectionCount = 8; // Test with 8 connections (1 batch)
    for (size_t i = 0; i < testAdhesions.activeConnectionCount; ++i) {
        testAdhesions.cellAIndex[i] = static_cast<uint32_t>(i);
        testAdhesions.cellBIndex[i] = static_cast<uint32_t>(i + 1);
        testAdhesions.modeIndex[i] = 0;
        testAdhesions.isActive[i] = 1;
        testAdhesions.zoneA[i] = 0;
        testAdhesions.zoneB[i] = 0;
        
        // Set anchor directions
        testAdhesions.anchorDirectionA_x[i] = 1.0f;
        testAdhesions.anchorDirectionA_y[i] = 0.0f;
        testAdhesions.anchorDirectionA_z[i] = 0.0f;
        testAdhesions.anchorDirectionB_x[i] = -1.0f;
        testAdhesions.anchorDirectionB_y[i] = 0.0f;
        testAdhesions.anchorDirectionB_z[i] = 0.0f;
        
        // Set twist references (identity quaternions)
        testAdhesions.twistReferenceA_w[i] = 1.0f;
        testAdhesions.twistReferenceA_x[i] = 0.0f;
        testAdhesions.twistReferenceA_y[i] = 0.0f;
        testAdhesions.twistReferenceA_z[i] = 0.0f;
        testAdhesions.twistReferenceB_w[i] = 1.0f;
        testAdhesions.twistReferenceB_x[i] = 0.0f;
        testAdhesions.twistReferenceB_y[i] = 0.0f;
        testAdhesions.twistReferenceB_z[i] = 0.0f;
    }
    
    // Initialize test mode settings
    GPUModeAdhesionSettings testMode{};
    testMode.restLength = 1.5f;
    testMode.linearSpringStiffness = 100.0f;
    testMode.linearSpringDamping = 0.1f;
    testMode.orientationSpringStiffness = 50.0f;
    testMode.orientationSpringDamping = 0.05f;
    testMode.enableTwistConstraint = 0;
    testModeSettings.push_back(testMode);
    
    // Test SIMD batch processing
    auto startTime = std::chrono::steady_clock::now();
    m_simdBatchProcessor->processAllConnections(testCells, testAdhesions, testModeSettings);
    auto endTime = std::chrono::steady_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    float processingTime = duration.count() / 1000.0f; // Convert to milliseconds
    
    std::cout << "SIMD Batch Processing Time: " << processingTime << " ms" << std::endl;
    std::cout << "Connections Processed: " << testAdhesions.activeConnectionCount << std::endl;
    std::cout << "Batches Processed: " << (testAdhesions.activeConnectionCount + 7) / 8 << std::endl;
    
    // Validate results
    bool hasForces = false;
    for (size_t i = 0; i < testCells.activeCellCount; ++i) {
        if (std::abs(testCells.acc_x[i]) > 0.001f || 
            std::abs(testCells.acc_y[i]) > 0.001f || 
            std::abs(testCells.acc_z[i]) > 0.001f) {
            hasForces = true;
            break;
        }
    }
    
    if (hasForces) {
        std::cout << "✓ SIMD Batch Processor Test PASSED - Forces were applied" << std::endl;
    } else {
        std::cout << "✗ SIMD Batch Processor Test FAILED - No forces were applied" << std::endl;
    }
    
    // Test precision validation
    bool precisionValid = m_simdBatchProcessor->validateSIMDPrecision(testCells, testAdhesions, testModeSettings, 0);
    if (precisionValid) {
        std::cout << "✓ SIMD Precision Validation PASSED" << std::endl;
    } else {
        std::cout << "✗ SIMD Precision Validation FAILED" << std::endl;
    }
    
    std::cout << "=== SIMD Batch Processor Test Complete ===" << std::endl;
}

void CPUSIMDPhysicsEngine::validateSIMDImplementation() {
    std::cout << "=== SIMD Implementation Validation ===" << std::endl;
    
    // Check SIMD alignment
    if (alignof(SIMDAdhesionBatchProcessor::PreallocatedBuffers) >= 32) {
        std::cout << "✓ SIMD Buffer Alignment: 32-byte aligned" << std::endl;
    } else {
        std::cout << "✗ SIMD Buffer Alignment: Not properly aligned" << std::endl;
    }
    
    // Check buffer sizes
    constexpr size_t expectedBufferSize = SIMDAdhesionBatchProcessor::SIMD_WIDTH;
    if (SIMDAdhesionBatchProcessor::PreallocatedBuffers{}.temp_posA_x.size() == expectedBufferSize) {
        std::cout << "✓ Buffer Sizes: Correct SIMD width (" << expectedBufferSize << ")" << std::endl;
    } else {
        std::cout << "✗ Buffer Sizes: Incorrect SIMD width" << std::endl;
    }
    
    // Check batch count calculation
    constexpr size_t expectedBatchCount = 5120 / 8; // 640 batches
    if (SIMDAdhesionBatchProcessor::BATCH_COUNT == expectedBatchCount) {
        std::cout << "✓ Batch Count: " << expectedBatchCount << " batches for 5,120 connections" << std::endl;
    } else {
        std::cout << "✗ Batch Count: Incorrect batch count calculation" << std::endl;
    }
    
    // Check maximum connections
    if (SIMDAdhesionBatchProcessor::MAX_CONNECTIONS == 5120) {
        std::cout << "✓ Maximum Connections: 5,120 (20 × 256 cells)" << std::endl;
    } else {
        std::cout << "✗ Maximum Connections: Incorrect maximum" << std::endl;
    }
    
    // Test basic SIMD operations
    alignas(32) std::array<float, 8> testA = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f};
    alignas(32) std::array<float, 8> testB = {2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f};
    alignas(32) std::array<float, 8> result;
    
    __m256 a = _mm256_load_ps(testA.data());
    __m256 b = _mm256_load_ps(testB.data());
    __m256 sum = _mm256_add_ps(a, b);
    _mm256_store_ps(result.data(), sum);
    
    bool simdWorking = true;
    for (size_t i = 0; i < 8; ++i) {
        if (std::abs(result[i] - (testA[i] + testB[i])) > 0.001f) {
            simdWorking = false;
            break;
        }
    }
    
    if (simdWorking) {
        std::cout << "✓ SIMD Operations: AVX2 instructions working correctly" << std::endl;
    } else {
        std::cout << "✗ SIMD Operations: AVX2 instructions not working correctly" << std::endl;
    }
    
    std::cout << "=== SIMD Implementation Validation Complete ===" << std::endl;
}

