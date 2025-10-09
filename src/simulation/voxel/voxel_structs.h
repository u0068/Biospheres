#pragma once
#include <glm/glm.hpp>
#include <cstdint>

// Voxel data structure for GPU storage
// 16³ voxel grid for nutrients and future fluid simulation
// Each voxel is 4x4x4 spatial grid cells (consistent size for fluid dynamics)
struct VoxelData
{
    glm::vec4 nutrientDensity;    // RGBA: different nutrient types (r,g,b) + total density (a)
    glm::vec4 positionAndSize;    // xyz: world position, w: voxel size
    glm::vec4 colorAndAlpha;      // RGB: visualization color, A: fade alpha
    float lifetime;               // Current lifetime (for decay)
    float maxLifetime;            // Maximum lifetime before decay starts
    uint32_t isActive;            // 1 if voxel has nutrients, 0 otherwise
    uint32_t _padding;            // Alignment padding (removed subdivisionLevel for consistent grid)
};
static_assert(sizeof(VoxelData) % 16 == 0, "VoxelData must be 16-byte aligned for GPU usage");

// Cloud generation parameters
struct CloudGenerationParams
{
    glm::vec3 centerPosition;     // Center of the nutrient cloud
    float radius;                 // Radius of the cloud
    float noiseScale;             // Scale of Perlin noise
    float noiseStrength;          // Strength of noise displacement
    float densityFalloff;         // How quickly density falls off from center
    float targetDensity;          // Peak density at center
    glm::vec3 color;              // Visual color of the cloud
    float fadeInDuration;         // Time to fade in (seconds)
    float sustainDuration;        // Time to sustain at full strength (seconds)
    float fadeOutDuration;        // Time to fade out (seconds)
    uint32_t cloudId;             // Unique identifier for this cloud
    float spawnTime;              // Time when this cloud was spawned
    uint32_t isActive;            // Whether this cloud is currently active
    uint32_t _padding[3];         // Alignment padding to reach 80 bytes (16-byte aligned)
};
static_assert(sizeof(CloudGenerationParams) % 16 == 0, "CloudGenerationParams must be 16-byte aligned");

// Voxel grid configuration
// 16³ grid covers same world space as 64³ spatial grid
// Each voxel = 4x4x4 spatial cells for performance
struct VoxelGridConfig
{
    int resolution;               // Voxel grid resolution (16 for nutrient/fluid)
    float voxelSize;              // Size of each voxel (worldSize / resolution)
    float worldSize;              // Total world size (matches spatial grid)
    int maxActiveVoxels;          // Maximum number of active voxels (sparse storage)
    float decayRate;              // Rate of nutrient decay over time
    float cloudSpawnInterval;     // Base interval between cloud spawns
    float cloudSpawnVariance;     // Random variance in spawn timing
    
    // Cloud generation parameters
    float noiseScale;             // Scale of Perlin noise (lower = larger features)
    float noiseStrength;          // Strength of noise displacement (0-1)
    float densityFalloff;         // How quickly density falls off from center
    float minCloudRadius;         // Minimum cloud radius
    float maxCloudRadius;         // Maximum cloud radius
    float nutrientDensityGradient; // Peak nutrient density at cloud center
    float nutrientDensityFalloff;  // How quickly nutrients fall off from center
};
