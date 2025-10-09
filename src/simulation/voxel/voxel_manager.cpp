#include "voxel_manager.h"
#include "../../rendering/camera/camera.h"
#include "../../core/config.h"
#include <GLFW/glfw3.h>
#include <random>
#include <cmath>
#include <iostream>
#include <cstring>

VoxelManager::VoxelManager()
{
    // Initialize default configuration
    // 16³ voxel grid (much coarser than 64³ spatial grid for performance)
    // Each voxel = 4x4x4 spatial cells
    config.resolution = config::VOXEL_BASE_RESOLUTION;
    config.voxelSize = config::WORLD_SIZE / config.resolution;
    config.worldSize = config::WORLD_SIZE;
    config.maxActiveVoxels = config::MAX_ACTIVE_VOXELS;
    config.decayRate = config::DEFAULT_DECAY_RATE;
    config.cloudSpawnInterval = config::DEFAULT_CLOUD_SPAWN_INTERVAL;
    config.cloudSpawnVariance = config::DEFAULT_CLOUD_SPAWN_VARIANCE;
    
    // Cloud generation defaults
    config.noiseScale = 0.05f;
    config.noiseStrength = 0.6f;
    config.densityFalloff = 1.5f;
    config.minCloudRadius = 15.0f;
    config.maxCloudRadius = 30.0f;
    config.nutrientDensityGradient = 25.0f;  // Peak density at center (25 × 4 channels = 100 total nutrients)
    config.nutrientDensityFalloff = 1.25f;   // Falloff rate
    
    timeSinceLastCloud = 0.0f;
    nextCloudSpawnTime = generateRandomFloat(
        config.cloudSpawnInterval - config.cloudSpawnVariance,
        config.cloudSpawnInterval + config.cloudSpawnVariance
    );
}

VoxelManager::~VoxelManager()
{
    cleanup();
}

void VoxelManager::initialize()
{
    initializeBuffers();
    initializeShaders();
    initializeRenderingGeometry();
    
    // Spawn initial test cloud at world center (larger and more irregular)
    spawnCloud(glm::vec3(0.0f, 0.0f, 0.0f), 25.0f, glm::vec3(1.0f, 0.3f, 0.8f));
    
    // Trigger immediate update to populate initial voxels
    if (cloudGenShader)
    {
        GLuint zero = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
        
        cloudGenShader->use();
        cloudGenShader->setFloat("u_deltaTime", 0.0f);
        cloudGenShader->setFloat("u_currentTime", static_cast<float>(glfwGetTime()));
        cloudGenShader->setInt("u_resolution", config.resolution);
        cloudGenShader->setFloat("u_voxelSize", config.voxelSize);
        cloudGenShader->setFloat("u_worldSize", config.worldSize);
        cloudGenShader->setInt("u_maxClouds", MAX_CLOUDS_OPTIMIZED);
        
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelDataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cloudParamsBuffer);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, voxelCountBuffer);
        
        int totalVoxels = config.resolution * config.resolution * config.resolution;
        glDispatchCompute((totalVoxels + 255) / 256, 1, 1);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
    }
    
    // Run initial compaction to get voxel count
    compactActiveVoxels();
}

void VoxelManager::cleanup()
{
    // Delete buffers
    if (voxelDataBuffer) glDeleteBuffers(1, &voxelDataBuffer);
    if (activeVoxelIndicesBuffer) glDeleteBuffers(1, &activeVoxelIndicesBuffer);
    if (cloudParamsBuffer) glDeleteBuffers(1, &cloudParamsBuffer);
    if (voxelCountBuffer) glDeleteBuffers(1, &voxelCountBuffer);
    if (indirectDrawBuffer) glDeleteBuffers(1, &indirectDrawBuffer);
    if (gridLineVBO) glDeleteBuffers(1, &gridLineVBO);
    if (voxelInstanceVBO) glDeleteBuffers(1, &voxelInstanceVBO);
    if (voxelInstanceEBO) glDeleteBuffers(1, &voxelInstanceEBO);
    if (gridLineVAO) glDeleteVertexArrays(1, &gridLineVAO);
    if (voxelInstanceVAO) glDeleteVertexArrays(1, &voxelInstanceVAO);
    
    // Delete particle buffers
    if (particleVAO) glDeleteVertexArrays(1, &particleVAO);
    if (particleBuffer) glDeleteBuffers(1, &particleBuffer);
    if (particleCountBuffer) glDeleteBuffers(1, &particleCountBuffer);
    if (particleIndirectBuffer) glDeleteBuffers(1, &particleIndirectBuffer);
    
    // Delete shaders
    delete cloudGenShader;
    delete decayShader;
    delete compactShader;
    delete particleGenShader;
    delete voxelRenderShader;
    delete particleShader;
}

void VoxelManager::reset()
{
    // Clear all voxels and particles
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    
    // Clear CPU-side voxel data
    for (auto& voxel : cpuVoxelData)
    {
        voxel = {}; // Zero-initialize
    }
    
    // Clear GPU voxel data buffer
    if (voxelDataBuffer != 0) {
        glClearNamedBufferData(voxelDataBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    
    // Clear active voxel indices
    if (activeVoxelIndicesBuffer != 0) {
        glClearNamedBufferData(activeVoxelIndicesBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    
    // Reset voxel count to zero
    if (voxelCountBuffer != 0) {
        GLuint zero = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    }
    
    // Reset indirect draw buffer (0 instances)
    if (indirectDrawBuffer != 0) {
        GLuint indirectCmd[5] = {36, 0, 0, 0, 0}; // 36 indices per cube, 0 instances
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(indirectCmd), indirectCmd);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }
    
    // Clear particle buffer
    if (particleBuffer != 0) {
        glClearNamedBufferData(particleBuffer, GL_R32F, GL_RED, GL_FLOAT, nullptr);
    }
    
    // Reset particle count to zero
    if (particleCountBuffer != 0) {
        GLuint zero = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, particleCountBuffer);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    }
    
    // Reset particle indirect draw buffer (0 instances)
    if (particleIndirectBuffer != 0) {
        GLuint indirectCmd[4] = {4, 0, 0, 0}; // 4 vertices per quad, 0 instances
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, particleIndirectBuffer);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0, sizeof(indirectCmd), indirectCmd);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }
    
    // Clear cloud params buffer
    if (cloudParamsBuffer != 0) {
        glClearNamedBufferData(cloudParamsBuffer, GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
    }
    
    // Reset cloud timing
    timeSinceLastCloud = 0.0f;
    nextCloudSpawnTime = generateRandomFloat(
        config.cloudSpawnInterval - config.cloudSpawnVariance,
        config.cloudSpawnInterval + config.cloudSpawnVariance
    );
    
    // Reset frame counters
    framesSinceCompact = 0;
    framesSinceNutrientSync = 0;
    timeSinceParticleUpdate = 0.0f;
}

void VoxelManager::initializeBuffers()
{
    // Calculate total voxels for 16³ grid
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    
    // Initialize CPU-side mirror of voxel data for fast sampling by cells
    cpuVoxelData.resize(totalVoxels);
    for (auto& voxel : cpuVoxelData)
    {
        voxel = {}; // Zero-initialize
    }
    
    // Voxel data buffer (SSBO)
    glGenBuffers(1, &voxelDataBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxelDataBuffer);
    
    // Initialize with zeros
    std::vector<VoxelData> zeroData(totalVoxels);
    memset(zeroData.data(), 0, totalVoxels * sizeof(VoxelData));
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 totalVoxels * sizeof(VoxelData), 
                 zeroData.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Active voxel indices buffer
    glGenBuffers(1, &activeVoxelIndicesBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, activeVoxelIndicesBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 config.maxActiveVoxels * sizeof(uint32_t), 
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Cloud parameters buffer (optimized for performance)
    glGenBuffers(1, &cloudParamsBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cloudParamsBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 
                 MAX_CLOUDS_OPTIMIZED * sizeof(CloudGenerationParams), 
                 nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Voxel count buffer (atomic counter)
    glGenBuffers(1, &voxelCountBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
    GLuint zero = 0;
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    
    // Indirect draw buffer (GPU manages count internally, no CPU readback)
    // Format: { count, instanceCount, firstIndex, baseVertex, baseInstance }
    glGenBuffers(1, &indirectDrawBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer);
    GLuint indirectCmd[5] = {36, 0, 0, 0, 0}; // 36 indices per cube, 0 instances initially
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(indirectCmd), indirectCmd, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}

void VoxelManager::initializeShaders()
{
    // Initialize compute shaders
    cloudGenShader = new Shader("shaders/voxel/cloud_generation.comp");
    decayShader = new Shader("shaders/voxel/decay.comp");
    compactShader = new Shader("shaders/voxel/compact_voxels.comp");
    particleGenShader = new Shader("shaders/voxel/generate_particles.comp");
    
    // Initialize rendering shaders
    gridLineShader = new Shader("shaders/voxel/grid_lines.vert", "shaders/voxel/grid_lines.frag");
    voxelRenderShader = new Shader("shaders/voxel/voxel_render.vert", "shaders/voxel/voxel_render.frag");
    particleShader = new Shader("shaders/voxel/nutrient_particle.vert", "shaders/voxel/nutrient_particle.frag");
}

void VoxelManager::initializeRenderingGeometry()
{
    generateGridLineGeometry();
    
    // Create cube mesh for instanced voxel rendering (filled volumetric cubes)
    float cubeVertices[] = {
        // Positions (unit cube centered at origin, 8 corners)
        -0.5f, -0.5f, -0.5f,  // 0
         0.5f, -0.5f, -0.5f,  // 1
         0.5f,  0.5f, -0.5f,  // 2
        -0.5f,  0.5f, -0.5f,  // 3
        -0.5f, -0.5f,  0.5f,  // 4
         0.5f, -0.5f,  0.5f,  // 5
         0.5f,  0.5f,  0.5f,  // 6
        -0.5f,  0.5f,  0.5f,  // 7
    };
    
    // Indices for 6 faces (36 indices for GL_TRIANGLES)
    unsigned int cubeIndices[] = {
        // Back face
        0, 1, 2,  2, 3, 0,
        // Front face
        4, 6, 5,  4, 7, 6,
        // Left face
        4, 0, 3,  3, 7, 4,
        // Right face
        1, 5, 6,  6, 2, 1,
        // Bottom face
        4, 5, 1,  1, 0, 4,
        // Top face
        3, 2, 6,  6, 7, 3
    };
    
    glGenVertexArrays(1, &voxelInstanceVAO);
    glGenBuffers(1, &voxelInstanceVBO);
    glGenBuffers(1, &voxelInstanceEBO);
    
    glBindVertexArray(voxelInstanceVAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, voxelInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, voxelInstanceEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);
    
    // Vertex position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Instance data will come from SSBO binding 1 (activeVoxelIndicesBuffer)
    // No need to set up attributes for it - the shader uses gl_InstanceID to index into it
    
    // Unbind VAO first (good practice)
    glBindVertexArray(0);
    
    // Unbind other buffers to avoid unintended modifications
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    // Initialize particle system (GPU-generated)
    // One particle per voxel (16³ = 4,096 instead of 64³ = 262,144)
    int maxParticles = config.resolution * config.resolution * config.resolution;
    
    // Particle buffer (SSBO) - generated by compute shader
    glGenBuffers(1, &particleBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, maxParticles * sizeof(glm::vec4), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Particle count buffer (atomic counter)
    glGenBuffers(1, &particleCountBuffer);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, particleCountBuffer);
    GLuint zero = 0;
    glBufferData(GL_ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    
    // Indirect draw buffer for particles (avoids CPU-GPU sync)
    // Format: {count, instanceCount, first, baseInstance}
    glGenBuffers(1, &particleIndirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, particleIndirectBuffer);
    GLuint indirectCmd[4] = {4, 0, 0, 0}; // 4 vertices per quad, 0 instances initially
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(indirectCmd), indirectCmd, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    
    // Setup VAO for rendering particles
    glGenVertexArrays(1, &particleVAO);
    glBindVertexArray(particleVAO);
    
    // Bind particle buffer as vertex attribute source
    glBindBuffer(GL_ARRAY_BUFFER, particleBuffer);
    
    // Attribute 0: position (xyz) and nutrient density (w) - instanced per particle
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribDivisor(0, 1); // Advance once per instance (particle)
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VoxelManager::generateGridLineGeometry()
{
    std::vector<glm::vec3> lineVertices;
    float halfWorld = config.worldSize * 0.5f;
    float step = config.voxelSize;
    
    // Generate complete 32³ grid lines (all internal grid lines)
    for (int i = 0; i <= config.resolution; ++i)
    {
        float pos = -halfWorld + i * step;
        
        // Lines parallel to X axis (in YZ plane at x=pos)
        for (int j = 0; j <= config.resolution; ++j)
        {
            float pos2 = -halfWorld + j * step;
            // Horizontal line at height pos2
            lineVertices.push_back(glm::vec3(-halfWorld, pos, pos2));
            lineVertices.push_back(glm::vec3(halfWorld, pos, pos2));
        }
        
        // Lines parallel to Y axis (in XZ plane at y=pos)
        for (int j = 0; j <= config.resolution; ++j)
        {
            float pos2 = -halfWorld + j * step;
            // Vertical line at depth pos2
            lineVertices.push_back(glm::vec3(pos, -halfWorld, pos2));
            lineVertices.push_back(glm::vec3(pos, halfWorld, pos2));
        }
        
        // Lines parallel to Z axis (in XY plane at z=pos)
        for (int j = 0; j <= config.resolution; ++j)
        {
            float pos2 = -halfWorld + j * step;
            // Depth line at horizontal pos2
            lineVertices.push_back(glm::vec3(pos, pos2, -halfWorld));
            lineVertices.push_back(glm::vec3(pos, pos2, halfWorld));
        }
    }
    
    glGenVertexArrays(1, &gridLineVAO);
    glGenBuffers(1, &gridLineVBO);
    
    glBindVertexArray(gridLineVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridLineVBO);
    glBufferData(GL_ARRAY_BUFFER, 
                 lineVertices.size() * sizeof(glm::vec3), 
                 lineVertices.data(), GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
}

void VoxelManager::update(float deltaTime)
{
    updateClouds(deltaTime);
    
    // Only compact periodically instead of every frame
    framesSinceCompact++;
    if (framesSinceCompact >= COMPACT_INTERVAL)
    {
        // Minimal barrier - only for shader storage
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
        
        compactActiveVoxels();
        framesSinceCompact = 0;
    }
    
    // Always run decay (GPU handles empty check internally)
    updateDecay(deltaTime);
    
    // Periodically sync nutrient data from GPU to CPU for cell sampling
    framesSinceNutrientSync++;
    if (framesSinceNutrientSync >= NUTRIENT_SYNC_INTERVAL)
    {
        syncNutrientDataToCPU();
        framesSinceNutrientSync = 0;
    }
    
    // Time-based particle updates (follows simulation speed)
    if (showNutrientParticles)
    {
        timeSinceParticleUpdate += deltaTime;
        if (timeSinceParticleUpdate >= PARTICLE_UPDATE_INTERVAL)
        {
            updateParticleData();
            timeSinceParticleUpdate = 0.0f;
        }
    }
}

void VoxelManager::updateClouds(float deltaTime)
{
    timeSinceLastCloud += deltaTime;
    
    // Check if it's time to spawn a new cloud (no limit on cloud count)
    if (timeSinceLastCloud >= nextCloudSpawnTime)
    {
        // Generate random cloud parameters using config values
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> posDistr(-config.worldSize * 0.35f, config.worldSize * 0.35f);
        std::uniform_real_distribution<float> radiusDistr(config.minCloudRadius, config.maxCloudRadius);
        std::uniform_real_distribution<float> colorDistr(0.3f, 1.0f);
        
        glm::vec3 center(posDistr(gen), posDistr(gen), posDistr(gen));
        float radius = radiusDistr(gen);
        glm::vec3 color(colorDistr(gen), colorDistr(gen), colorDistr(gen));
        
        spawnCloud(center, radius, color);
        
        // Schedule next cloud
        timeSinceLastCloud = 0.0f;
        nextCloudSpawnTime = generateRandomFloat(
            config.cloudSpawnInterval - config.cloudSpawnVariance,
            config.cloudSpawnInterval + config.cloudSpawnVariance
        );
    }
    
    // Update cloud generation shader every frame (needed for variable simulation speeds)
    if (cloudGenShader && cloudsAreDirty)
    {
        cloudsAreDirty = false;
        
        // Reset active voxel counter before cloud generation
        GLuint zero = 0;
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
        glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
        glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
        
        cloudGenShader->use();
        cloudGenShader->setFloat("u_deltaTime", deltaTime);
        cloudGenShader->setFloat("u_currentTime", static_cast<float>(glfwGetTime()));
        cloudGenShader->setInt("u_resolution", config.resolution);
        cloudGenShader->setFloat("u_voxelSize", config.voxelSize);
        cloudGenShader->setFloat("u_worldSize", config.worldSize);
        cloudGenShader->setInt("u_maxClouds", MAX_CLOUDS_OPTIMIZED);
        
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelDataBuffer);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, cloudParamsBuffer);
        glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, voxelCountBuffer);
        
        // Process all voxels to generate clouds (32^3 = 32,768 voxels)
        int totalVoxels = config.resolution * config.resolution * config.resolution;
        glDispatchCompute((totalVoxels + 255) / 256, 1, 1);
        
        // Minimal barrier - only for shader storage, no glFinish()
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
    }
}

void VoxelManager::updateDecay(float deltaTime)
{
    if (!decayShader) return;
    
    decayShader->use();
    decayShader->setFloat("u_deltaTime", deltaTime);
    decayShader->setFloat("u_decayRate", config.decayRate);
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelDataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, activeVoxelIndicesBuffer);
    
    // Process max possible voxels (GPU will early-exit on inactive ones)
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    glDispatchCompute((totalVoxels + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
}

void VoxelManager::compactActiveVoxels()
{
    // Compact the active voxel list by removing inactive voxels
    // This will be handled by the compact compute shader
    if (!compactShader) return;
    
    // Reset counter
    GLuint zero = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, voxelCountBuffer);
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
    
    compactShader->use();
    compactShader->setInt("u_maxVoxels", config.maxActiveVoxels);
    
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelDataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, activeVoxelIndicesBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, voxelCountBuffer);
    
    // Must scan ALL voxels (32^3 = 32,768), not just maxActiveVoxels
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    glDispatchCompute((totalVoxels + 255) / 256, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
    
    // Copy voxel count to indirect draw buffer (GPU-side only, no readback!)
    // Copies voxelCountBuffer[0] -> indirectDrawBuffer[1] (instanceCount field)
    glBindBuffer(GL_COPY_READ_BUFFER, voxelCountBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, indirectDrawBuffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                        0, sizeof(GLuint), sizeof(GLuint));
    
    // Ensure the copy is complete before we try to draw
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT);
    
    // Unbind
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
}

void VoxelManager::syncNutrientDataToCPU()
{
    // Read voxel data from GPU to CPU for cell sampling
    // This is done periodically (every 10 frames) to minimize GPU-CPU sync overhead
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, voxelDataBuffer);
    void* mappedData = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, 
                                        totalVoxels * sizeof(VoxelData), 
                                        GL_MAP_READ_BIT);
    
    if (mappedData)
    {
        memcpy(cpuVoxelData.data(), mappedData, totalVoxels * sizeof(VoxelData));
        glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    }
    else
    {
        std::cerr << "Failed to map voxel data buffer for reading!" << std::endl;
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void VoxelManager::renderVoxelGrid(const Camera& camera, const glm::vec2& resolution, bool showGridLines, bool showVoxels,
                                   float maxRenderDistance, float fadeStartDistance)
{
    if (showGridLines)
    {
        renderGridLines(camera, resolution);
    }
    
    if (showVoxels)
    {
        renderVolumetricVoxels(camera, resolution);
    }
    
    // Render nutrient particles (if enabled)
    renderNutrientParticles(camera, resolution, maxRenderDistance, fadeStartDistance);
}

void VoxelManager::renderGridLines(const Camera& camera, const glm::vec2& resolution)
{
    if (!gridLineShader) return;
    
    // Calculate projection matrix
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = camera.getViewMatrix();
    
    gridLineShader->use();
    gridLineShader->setMat4("u_view", view);
    gridLineShader->setMat4("u_projection", projection);
    gridLineShader->setVec3("u_lineColor", glm::vec3(0.3f, 0.3f, 0.3f));
    gridLineShader->setFloat("u_lineAlpha", 0.2f);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);  // Don't write to depth buffer for transparent lines
    
    glBindVertexArray(gridLineVAO);
    
    // Calculate number of vertices for complete 32³ grid
    // Each axis: (resolution+1) * (resolution+1) lines, 2 vertices each
    int totalLines = 3 * (config.resolution + 1) * (config.resolution + 1) * 2;
    glDrawArrays(GL_LINES, 0, totalLines);
    
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glBindVertexArray(0);
}

void VoxelManager::renderVolumetricVoxels(const Camera& camera, const glm::vec2& resolution)
{
    if (!voxelRenderShader) 
    {
        std::cerr << "Voxel render shader not loaded!" << std::endl;
        return;
    }
    
    // Calculate projection matrix
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = camera.getViewMatrix();
    
    // Set up shader
    voxelRenderShader->use();
    voxelRenderShader->setMat4("u_view", view);
    voxelRenderShader->setMat4("u_projection", projection);
    voxelRenderShader->setVec3("u_cameraPos", camera.getPosition());
    voxelRenderShader->setFloat("u_colorSensitivity", colorSensitivity);
    
    // Enable depth testing for opaque voxels
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // Bind VAO and SSBOs
    glBindVertexArray(voxelInstanceVAO);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelDataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, activeVoxelIndicesBuffer);
    
    // Ensure all SSBO writes are complete before drawing
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_COMMAND_BARRIER_BIT);
    
    // Bind indirect draw buffer and draw
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectDrawBuffer);
    glDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0);
    
    // Cleanup
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
}

void VoxelManager::updateParticleData()
{
    if (!particleGenShader)
        return;
    
    // Reset particle counter
    GLuint zero = 0;
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, particleCountBuffer);
    glBufferSubData(GL_ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);
    
    // Setup compute shader
    particleGenShader->use();
    particleGenShader->setInt("u_voxelResolution", config.resolution);
    particleGenShader->setFloat("u_worldSize", config.worldSize);
    particleGenShader->setFloat("u_particleJitter", particleJitter);
    
    // Bind buffers
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, voxelDataBuffer);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, particleBuffer);
    glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 0, particleCountBuffer);
    
    // Dispatch compute shader (one thread per voxel)
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    int workGroups = (totalVoxels + 255) / 256;
    glDispatchCompute(workGroups, 1, 1);
    
    // Wait for compute shader to finish
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_ATOMIC_COUNTER_BARRIER_BIT);
    
    // Copy particle count from atomic counter to indirect draw buffer (GPU-side, no CPU stall)
    // Indirect draw format: {vertexCount, instanceCount, firstVertex, baseInstance}
    // We need to copy the atomic counter value to the instanceCount field (offset 4 bytes)
    glBindBuffer(GL_COPY_READ_BUFFER, particleCountBuffer);
    glBindBuffer(GL_COPY_WRITE_BUFFER, particleIndirectBuffer);
    glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, sizeof(GLuint), sizeof(GLuint));
    
    glBindBuffer(GL_COPY_READ_BUFFER, 0);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);
}

void VoxelManager::renderNutrientParticles(const Camera& camera, const glm::vec2& resolution, 
                                           float maxRenderDistance, float fadeStartDistance)
{
    if (!particleShader || !showNutrientParticles)
    {
        return;
    }
    
    // Calculate projection matrix
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio))
    {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    glm::mat4 view = camera.getViewMatrix();
    
    // Set up shader
    particleShader->use();
    particleShader->setMat4("u_view", view);
    particleShader->setMat4("u_projection", projection);
    particleShader->setVec3("u_cameraPos", camera.getPosition());
    particleShader->setFloat("u_particleSize", particleSize);
    particleShader->setFloat("u_colorSensitivity", colorSensitivity);
    
    // Distance culling and fading (use configurable values from CellManager)
    particleShader->setFloat("u_cullDistance", maxRenderDistance);
    particleShader->setFloat("u_fadeStartDistance", fadeStartDistance);
    
    // Enable blending for transparent particles
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending - order-independent, no flicker
    glDepthMask(GL_FALSE); // Don't write to depth buffer for transparent particles
    glEnable(GL_DEPTH_TEST); // Enable depth testing so particles respect depth from cells
    
    // Render particles as instanced quads using indirect draw (no CPU-GPU sync)
    glBindVertexArray(particleVAO);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, particleIndirectBuffer);
    glDrawArraysIndirect(GL_TRIANGLE_STRIP, 0);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glBindVertexArray(0);
    
    // Restore rendering state
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void VoxelManager::spawnCloud(const glm::vec3& center, float radius, const glm::vec3& color)
{
    CloudGenerationParams cloud{};
    cloud.centerPosition = center;
    cloud.radius = radius;
    cloud.noiseScale = config.noiseScale;
    cloud.noiseStrength = config.noiseStrength;
    cloud.densityFalloff = config.nutrientDensityFalloff;
    cloud.targetDensity = config.nutrientDensityGradient;
    cloud.color = color;
    cloud.fadeInDuration = 2.0f;
    cloud.sustainDuration = 20.0f;
    cloud.fadeOutDuration = 3.0f;
    cloud.cloudId = nextCloudId++;
    cloud.spawnTime = static_cast<float>(glfwGetTime());
    cloud.isActive = 1;
    
    // Find first available slot (reduced to MAX_CLOUDS_OPTIMIZED)
    static int nextCloudSlot = 0;
    int cloudSlot = nextCloudSlot;
    nextCloudSlot = (nextCloudSlot + 1) % MAX_CLOUDS_OPTIMIZED;
    
    // Upload cloud to GPU at the selected slot
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, cloudParamsBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 
                    cloudSlot * sizeof(CloudGenerationParams),
                    sizeof(CloudGenerationParams),
                    &cloud);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Mark clouds as dirty so they'll be regenerated
    cloudsAreDirty = true;
}

glm::vec4 VoxelManager::sampleNutrientAt(const glm::vec3& worldPos)
{
    // Sample nutrient density from CPU-side mirror (synced periodically from GPU)
    glm::ivec3 voxelCoord = worldToVoxelCoord(worldPos);
    int index = voxelCoordToIndex(voxelCoord);
    
    // Bounds check
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    if (index < 0 || index >= totalVoxels || cpuVoxelData.empty())
    {
        return glm::vec4(0.0f);
    }
    
    const VoxelData& voxel = cpuVoxelData[index];
    
    // Only return nutrients if the voxel is active
    if (voxel.isActive)
    {
        return voxel.nutrientDensity;
    }
    
    return glm::vec4(0.0f);
}

void VoxelManager::consumeNutrientAt(const glm::vec3& worldPos, float amount)
{
    // Reduce nutrient density in CPU-side mirror
    // The GPU continues to handle generation and decay independently
    // CPU-side consumption is for immediate feedback to cells
    
    glm::ivec3 voxelCoord = worldToVoxelCoord(worldPos);
    int index = voxelCoordToIndex(voxelCoord);
    
    // Bounds check
    int totalVoxels = config.resolution * config.resolution * config.resolution;
    if (index < 0 || index >= totalVoxels || cpuVoxelData.empty())
    {
        return;
    }
    
    VoxelData& voxel = cpuVoxelData[index];
    
    // Only consume if the voxel is active
    if (voxel.isActive)
    {
        // Reduce all nutrient channels by the consumption amount
        voxel.nutrientDensity = glm::max(voxel.nutrientDensity - glm::vec4(amount), glm::vec4(0.0f));
        
        // If all nutrients are depleted, mark as inactive
        float totalNutrient = voxel.nutrientDensity.r + voxel.nutrientDensity.g + 
                              voxel.nutrientDensity.b + voxel.nutrientDensity.a;
        if (totalNutrient < 0.001f)
        {
            voxel.isActive = 0;
        }
    }
}

glm::ivec3 VoxelManager::worldToVoxelCoord(const glm::vec3& worldPos) const
{
    float halfWorld = config.worldSize * 0.5f;
    glm::vec3 normalized = (worldPos + halfWorld) / config.worldSize;
    glm::ivec3 coord = glm::ivec3(normalized * float(config.resolution));
    return glm::clamp(coord, glm::ivec3(0), glm::ivec3(config.resolution - 1));
}

int VoxelManager::voxelCoordToIndex(const glm::ivec3& coord) const
{
    return coord.x + coord.y * config.resolution + coord.z * config.resolution * config.resolution;
}

glm::vec3 VoxelManager::voxelIndexToWorldPos(int index) const
{
    int z = index / (config.resolution * config.resolution);
    int y = (index / config.resolution) % config.resolution;
    int x = index % config.resolution;
    
    float halfWorld = config.worldSize * 0.5f;
    return glm::vec3(
        -halfWorld + (x + 0.5f) * config.voxelSize,
        -halfWorld + (y + 0.5f) * config.voxelSize,
        -halfWorld + (z + 0.5f) * config.voxelSize
    );
}

// Convert spatial grid coordinate (64³) to voxel grid coordinate (16³)
// Each voxel = 4x4x4 spatial cells
glm::ivec3 VoxelManager::spatialToVoxelCoord(const glm::ivec3& spatialCoord) const
{
    // Spatial grid is 64³, voxel grid is 16³
    // Divide by 4 to get voxel coordinate
    return spatialCoord / 4;
}

// Convert voxel grid coordinate (16³) to spatial grid coordinate (64³)
// Returns the lower corner of the 4x4x4 spatial cell block
glm::ivec3 VoxelManager::voxelToSpatialCoord(const glm::ivec3& voxelCoord) const
{
    // Multiply by 4 to get spatial coordinate
    return voxelCoord * 4;
}

float VoxelManager::generateRandomFloat(float min, float max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(min, max);
    return dist(gen);
}
