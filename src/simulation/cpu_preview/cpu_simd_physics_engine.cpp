#include "cpu_simd_physics_engine.h"
#include <chrono>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <functional>
#include <intrin.h> // For _mm_prefetch on MSVC

CPUSIMDPhysicsEngine::CPUSIMDPhysicsEngine() 
{
    // Only create spatial grid if not in preview mode
    if (!m_previewMode) {
        m_spatialGrid = std::make_unique<CPUSpatialGrid>();
        m_spatialGrid->initialize();
        m_spatialGrid->clear();
    }
    
    // Initialize preallocated buffers to avoid dynamic allocation
    m_buffers.initialize();
}

CPUSIMDPhysicsEngine::~CPUSIMDPhysicsEngine() {
    // Cleanup handled by RAII
}

void CPUSIMDPhysicsEngine::simulateStep(CPUCellPhysics_SoA& cells, 
                                       CPUAdhesionConnections_SoA& adhesions,
                                       float deltaTime) {
    simulateStep(cells, adhesions, deltaTime, nullptr);
}

void CPUSIMDPhysicsEngine::simulateStep(CPUCellPhysics_SoA& cells, 
                                       CPUAdhesionConnections_SoA& adhesions,
                                       float deltaTime,
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
        calculateAdhesionForces(cells, adhesions);
        
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
    
    for (size_t batchStart = 0; batchStart < cells.activeCellCount; batchStart += batchSize) {
        size_t batchCount = std::min(batchSize, cells.activeCellCount - batchStart);
        processCellBatch(cells, batchStart, batchCount);
    }
    
    // Apply GPU-style acceleration damping (behavioral equivalence)
    applyAccelerationDamping(cells);
}

void CPUSIMDPhysicsEngine::calculateAdhesionForces(CPUCellPhysics_SoA& cells, 
                                                   const CPUAdhesionConnections_SoA& adhesions) {
    // Skip adhesion forces if no connections (major optimization)
    if (adhesions.activeConnectionCount == 0) {
        return;
    }
    
    // Process adhesion connections with SIMD optimization where possible
    const size_t batchSize = SIMD_WIDTH;
    size_t processed = 0;
    
    // Process connections in SIMD batches
    while (processed + batchSize <= adhesions.activeConnectionCount) {
        simd_adhesion_force_batch(cells, adhesions, processed, batchSize);
        processed += batchSize;
    }
    
    // Process remaining connections individually
    for (size_t i = processed; i < adhesions.activeConnectionCount; ++i) {
        processAdhesionConnection(static_cast<uint32_t>(i), adhesions, cells);
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
    }
}

void CPUSIMDPhysicsEngine::updateOrientations(CPUCellPhysics_SoA& cells, float deltaTime) {
    // Simple orientation update based on velocity direction
    for (size_t i = 0; i < cells.activeCellCount; ++i) {
        glm::vec3 velocity(cells.vel_x[i], cells.vel_y[i], cells.vel_z[i]);
        float speed = glm::length(velocity);
        
        if (speed > 0.001f) {
            // Gradually align orientation with velocity direction
            glm::vec3 forward = velocity / speed;
            
            // Simple quaternion update (could be more sophisticated)
            // For now, just maintain the current orientation
            // Full implementation would calculate proper quaternion rotation
        }
    }
}

void CPUSIMDPhysicsEngine::updateSpatialGrid(const CPUCellPhysics_SoA& cells) {
    // Skip spatial grid entirely in preview mode (max 256 cells)
    if (m_previewMode || !m_spatialGrid) {
        return;
    }
    
    // Skip spatial grid for small cell counts (direct collision is faster)
    if (cells.activeCellCount <= 8) {
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
    
    // Always use direct collision detection in preview mode (max 256 cells)
    if (m_previewMode || !m_spatialGrid || cells.activeCellCount <= 8) {
        for (size_t i = startIdx; i < endIdx; ++i) {
            for (size_t j = i + 1; j < cells.activeCellCount; ++j) {
                processCollisionPair(static_cast<uint32_t>(i), static_cast<uint32_t>(j), cells);
            }
        }
        return;
    }
    
    // Use spatial grid for larger cell counts in non-preview mode
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
    glm::vec3 myPos(cells.pos_x[cellA], cells.pos_y[cellA], cells.pos_z[cellA]);
    glm::vec3 otherPos(cells.pos_x[cellB], cells.pos_y[cellB], cells.pos_z[cellB]);
    
    float myMass = cells.mass[cellA];
    float otherMass = cells.mass[cellB];
    
    // GPU algorithm: radius = pow(mass, 1/3)
    float myRadius = std::pow(myMass, 1.0f/3.0f);
    float otherRadius = std::pow(otherMass, 1.0f/3.0f);
    
    glm::vec3 delta = myPos - otherPos;
    float distance = glm::length(delta);
    
    // Early distance check (GPU optimization)
    if (distance > 4.0f) {
        return; // Approximate max interaction distance
    }
    
    float minDistance = myRadius + otherRadius;
    
    if (distance < minDistance && distance > 0.001f) {
        // Collision detected - apply repulsion force (identical to GPU)
        glm::vec3 direction = glm::normalize(delta);
        float overlap = minDistance - distance;
        float hardness = 10.0f; // GPU constant
        glm::vec3 totalForce = direction * overlap * hardness;
        
        // Calculate acceleration (F = ma, so a = F/m) - GPU algorithm
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
                                                    CPUCellPhysics_SoA& cells) {
    // Behavioral equivalence with GPU: adhesion_physics.comp algorithm
    uint32_t cellA = adhesions.cellA_indices[connectionIndex];
    uint32_t cellB = adhesions.cellB_indices[connectionIndex];
    
    if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount) {
        return; // Invalid connection
    }
    
    // Load cell data (matching GPU ComputeCell structure)
    glm::vec3 posA(cells.pos_x[cellA], cells.pos_y[cellA], cells.pos_z[cellA]);
    glm::vec3 posB(cells.pos_x[cellB], cells.pos_y[cellB], cells.pos_z[cellB]);
    glm::vec3 velA(cells.vel_x[cellA], cells.vel_y[cellA], cells.vel_z[cellA]);
    glm::vec3 velB(cells.vel_x[cellB], cells.vel_y[cellB], cells.vel_z[cellB]);
    
    float massA = cells.mass[cellA];
    float massB = cells.mass[cellB];
    
    // Connection vector from A to B (GPU algorithm)
    glm::vec3 deltaPos = posB - posA;
    float dist = glm::length(deltaPos);
    if (dist < 0.0001f) return;
    
    glm::vec3 adhesionDir = deltaPos / dist;
    float restLength = adhesions.rest_length[connectionIndex];
    float stiffness = adhesions.stiffness[connectionIndex];
    
    // Linear spring force (GPU algorithm)
    float forceMag = stiffness * (dist - restLength);
    glm::vec3 springForce = adhesionDir * forceMag;
    
    // Damping - oppose relative motion (GPU algorithm)
    glm::vec3 relVel = velB - velA;
    float dampingCoeff = 0.1f; // Default damping from GPU
    float dampMag = 1.0f - dampingCoeff * glm::dot(relVel, adhesionDir);
    glm::vec3 dampingForce = -adhesionDir * dampMag;
    
    glm::vec3 totalForceA = springForce + dampingForce;
    glm::vec3 totalForceB = -(springForce + dampingForce);
    
    // Apply forces as accelerations (F = ma, so a = F/m)
    if (massA > 0.0f) {
        glm::vec3 accA = totalForceA / massA;
        cells.acc_x[cellA] += accA.x;
        cells.acc_y[cellA] += accA.y;
        cells.acc_z[cellA] += accA.z;
    }
    
    if (massB > 0.0f) {
        glm::vec3 accB = totalForceB / massB;
        cells.acc_x[cellB] += accB.x;
        cells.acc_y[cellB] += accB.y;
        cells.acc_z[cellB] += accB.z;
    }
}

void CPUSIMDPhysicsEngine::simd_adhesion_force_batch(CPUCellPhysics_SoA& cells,
                                                    const CPUAdhesionConnections_SoA& adhesions,
                                                    size_t start_idx, size_t count) {
    // SIMD-optimized adhesion force calculation for a batch of connections
    // Process multiple adhesion connections simultaneously using AVX2
    
    const __m256 epsilon = _mm256_set1_ps(0.001f);
    
    // Process connections in the batch
    for (size_t i = 0; i < count; ++i) {
        size_t conn_idx = start_idx + i;
        
        uint32_t cellA = adhesions.cellA_indices[conn_idx];
        uint32_t cellB = adhesions.cellB_indices[conn_idx];
        
        if (cellA >= cells.activeCellCount || cellB >= cells.activeCellCount) {
            continue; // Invalid connection
        }
        
        // Load positions and properties
        float posA_x = cells.pos_x[cellA];
        float posA_y = cells.pos_y[cellA];
        float posA_z = cells.pos_z[cellA];
        
        float posB_x = cells.pos_x[cellB];
        float posB_y = cells.pos_y[cellB];
        float posB_z = cells.pos_z[cellB];
        
        float restLength = adhesions.rest_length[conn_idx];
        float stiffness = adhesions.stiffness[conn_idx];
        
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
            
            // Calculate force magnitude
            float extension = currentLength - restLength;
            float forceMagnitude = extension * stiffness;
            
            // Calculate force vector
            float force_x = dir_x * forceMagnitude;
            float force_y = dir_y * forceMagnitude;
            float force_z = dir_z * forceMagnitude;
            
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
    // This would be a separate compute shader in the GPU pipeline
    
    const __m256 dt_vec = _mm256_set1_ps(deltaTime);
    size_t simd_count = (cells.activeCellCount / SIMD_WIDTH) * SIMD_WIDTH;
    
    // SIMD velocity update
    for (size_t i = 0; i < simd_count; i += SIMD_WIDTH) {
        __m256 vel_x = _mm256_load_ps(&cells.vel_x[i]);
        __m256 vel_y = _mm256_load_ps(&cells.vel_y[i]);
        __m256 vel_z = _mm256_load_ps(&cells.vel_z[i]);
        
        __m256 acc_x = _mm256_load_ps(&cells.acc_x[i]);
        __m256 acc_y = _mm256_load_ps(&cells.acc_y[i]);
        __m256 acc_z = _mm256_load_ps(&cells.acc_z[i]);
        
        // GPU algorithm: vel += acc * deltaTime
        __m256 new_vel_x = _mm256_add_ps(vel_x, _mm256_mul_ps(acc_x, dt_vec));
        __m256 new_vel_y = _mm256_add_ps(vel_y, _mm256_mul_ps(acc_y, dt_vec));
        __m256 new_vel_z = _mm256_add_ps(vel_z, _mm256_mul_ps(acc_z, dt_vec));
        
        _mm256_store_ps(&cells.vel_x[i], new_vel_x);
        _mm256_store_ps(&cells.vel_y[i], new_vel_y);
        _mm256_store_ps(&cells.vel_z[i], new_vel_z);
    }
    
    // Handle remaining cells with scalar operations
    for (size_t i = simd_count; i < cells.activeCellCount; ++i) {
        // GPU algorithm: vel += acc * deltaTime
        cells.vel_x[i] += cells.acc_x[i] * deltaTime;
        cells.vel_y[i] += cells.acc_y[i] * deltaTime;
        cells.vel_z[i] += cells.acc_z[i] * deltaTime;
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
        if (cells.activeCellCount >= 255) { // CPU limit is 256, leave room for one more
            break; // No space available, cancel remaining splits
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
        
        cells.quat_x[daughterIndex] = cells.quat_x[cellIndex];
        cells.quat_y[daughterIndex] = cells.quat_y[cellIndex];
        cells.quat_z[daughterIndex] = cells.quat_z[cellIndex];
        cells.quat_w[daughterIndex] = cells.quat_w[cellIndex];
        
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
        
        // GPU behavior: Apply split direction with cell orientation
        // For now, use split direction directly (orientation transformation would be added later)
        glm::vec3 separationDirection = splitDirection;
        
        // GPU behavior: Move cells apart by 0.5 units in split direction
        glm::vec3 offset = separationDirection * 0.5f;
        
        // Child A gets +offset, Child B gets -offset (matching GPU)
        cells.pos_x[cellIndex] += offset.x;      // Child A (parent index)
        cells.pos_y[cellIndex] += offset.y;
        cells.pos_z[cellIndex] += offset.z;
        
        cells.pos_x[daughterIndex] -= offset.x;  // Child B (new index)
        cells.pos_y[daughterIndex] -= offset.y;
        cells.pos_z[daughterIndex] -= offset.z;
        
        // GPU behavior: No velocity separation - cells inherit parent velocity
    }
}