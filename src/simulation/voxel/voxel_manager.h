#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "voxel_structs.h"
#include "../../rendering/core/shader_class.h"

// Forward declarations
class Camera;

class VoxelManager
{
public:
    VoxelManager();
    ~VoxelManager();

    // Initialize voxel system
    void initialize();
    void cleanup();
    void reset(); // Reset all voxels and particles

    // Update voxel grid (decay, cloud generation)
    void update(float deltaTime);

    // Render voxel grid
    void renderVoxelGrid(const Camera& camera, const glm::vec2& resolution, bool showGridLines = true, bool showVoxels = true, 
                         float maxRenderDistance = 170.0f, float fadeStartDistance = 30.0f);

    // Query nutrient at a world position (for cells to sample from voxel grid)
    // Cells use 64³ spatial grid for neighbors, 16³ voxel grid for nutrients
    glm::vec4 sampleNutrientAt(const glm::vec3& worldPos);
    
    // Consume nutrients at a position (reduces nutrient density) - to be implemented with cell system
    void consumeNutrientAt(const glm::vec3& worldPos, float amount);
    
    // Convert between spatial grid coords and voxel grid coords
    glm::ivec3 spatialToVoxelCoord(const glm::ivec3& spatialCoord) const;
    glm::ivec3 voxelToSpatialCoord(const glm::ivec3& voxelCoord) const;

    // Manual cloud spawning (for testing)
    void spawnCloud(const glm::vec3& center, float radius, const glm::vec3& color);

    // Configuration accessors
    VoxelGridConfig& getConfig() { return config; }
    const VoxelGridConfig& getConfig() const { return config; }
    
    // Buffer accessors (for cell nutrient absorption)
    GLuint getVoxelDataBuffer() const { return voxelDataBuffer; }
    
    // Rendering parameters
    float colorSensitivity = 0.1f;   // Controls how sensitive voxel colors are to nutrient density
    float particleSize = 0.25f;      // Size of nutrient particles
    float particleJitter = 0.75f;    // Random offset for particles (0-2, where 1 = half cell size)
    bool showNutrientParticles = true; // Toggle particle visualization

private:
    // Configuration
    VoxelGridConfig config;

    // GPU buffers
    GLuint voxelDataBuffer{};           // SSBO for voxel data
    GLuint activeVoxelIndicesBuffer{};  // SSBO for indices of active voxels
    GLuint cloudParamsBuffer{};         // SSBO for cloud generation parameters
    GLuint voxelCountBuffer{};          // Atomic counter for active voxels
    GLuint indirectDrawBuffer{};        // Indirect draw command buffer (GPU-side only)
    
    // Rendering buffers
    GLuint gridLineVAO{};               // VAO for grid line rendering
    GLuint gridLineVBO{};               // VBO for grid line vertices
    GLuint voxelInstanceVAO{};          // VAO for instanced voxel rendering
    GLuint voxelInstanceVBO{};          // VBO for cube mesh
    GLuint voxelInstanceEBO{};          // EBO for cube indices
    
    // Compute shaders
    Shader* cloudGenShader = nullptr;   // Cloud generation compute shader
    Shader* decayShader = nullptr;      // Nutrient decay compute shader
    Shader* compactShader = nullptr;    // Compact active voxels compute shader
    Shader* particleGenShader = nullptr; // Particle generation compute shader
    
    // Rendering shaders
    Shader* gridLineShader = nullptr;   // Grid line rendering shader
    Shader* voxelRenderShader = nullptr; // Volumetric voxel rendering shader
    Shader* particleShader = nullptr;    // Nutrient particle shader

    // State tracking
    int nextCloudSlot{};              // Next slot in cloud buffer (append-only, wraps around)
    float timeSinceLastCloud{};
    float nextCloudSpawnTime{};
    uint32_t nextCloudId{1};
    
    // Performance optimization
    bool cloudsAreDirty{false};       // Track if clouds changed this frame
    int framesSinceCompact{0};        // Only compact every N frames
    int framesSinceNutrientSync{0};   // Only sync nutrient data to CPU every N frames
    float timeSinceParticleUpdate{0.0f}; // Time-based particle updates (follows simulation speed)
    
    static constexpr int COMPACT_INTERVAL = 30; // Compact every 30 frames (~0.5s at 60fps)
    static constexpr int NUTRIENT_SYNC_INTERVAL = 10; // Sync nutrients every 10 frames (~0.16s at 60fps)
    static constexpr float PARTICLE_UPDATE_INTERVAL = 0.016f; // Update particles every ~16ms for smooth transitions
    static constexpr int MAX_CLOUDS_OPTIMIZED = 100; // Maximum simultaneous clouds
    
    // CPU-side mirror of voxel nutrient data (for fast CPU sampling by cells)
    std::vector<VoxelData> cpuVoxelData;
    
    // Particle rendering data
    GLuint particleVAO{};
    GLuint particleBuffer{};          // SSBO for particle data
    GLuint particleCountBuffer{};     // Atomic counter for particle count
    GLuint particleIndirectBuffer{};  // Indirect draw buffer (avoids CPU-GPU sync)

    // Internal helper functions
    void initializeBuffers();
    void initializeShaders();
    void initializeRenderingGeometry();
    
    void updateDecay(float deltaTime);
    void updateClouds(float deltaTime);
    void compactActiveVoxels();
    void syncNutrientDataToCPU();     // Sync GPU voxel data to CPU for cell sampling
    
    void generateGridLineGeometry();
    void renderGridLines(const Camera& camera, const glm::vec2& resolution);
    void renderVolumetricVoxels(const Camera& camera, const glm::vec2& resolution);
    void updateParticleData();           // Update particle positions based on nutrient data
    void renderNutrientParticles(const Camera& camera, const glm::vec2& resolution, 
                                 float maxRenderDistance, float fadeStartDistance);
    
    glm::ivec3 worldToVoxelCoord(const glm::vec3& worldPos) const;
    int voxelCoordToIndex(const glm::ivec3& coord) const;
    glm::vec3 voxelIndexToWorldPos(int index) const;
    
    float generateRandomFloat(float min, float max);
};
