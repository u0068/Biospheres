#pragma once
#include <string_view>
#include <glm/glm.hpp>

namespace config
{
	// Application Configuration
	// This namespace contains all configuration constants, default values,
	// and "magic numbers" used throughout the application.

	// ========== Window and OpenGL Configuration ==========
	constexpr int INITIAL_WINDOW_WIDTH{800};
	constexpr int INITIAL_WINDOW_HEIGHT{600};
	constexpr int OPENGL_VERSION_MAJOR{4};
	constexpr int OPENGL_VERSION_MINOR{6};
	constexpr const char* GLSL_VERSION{"#version 460"};
	constexpr const char* APPLICATION_NAME{"Biospheres"};
	constexpr bool PLAY_STARTUP_JINGLE{false};
	constexpr bool VSYNC{ false };

	// ========== Cell Simulation Configuration ==========
	
	// System-Specific Capacity Constants
	// CPU Preview System: Limited to 256 cells for genome editing and preview
	// Used by CPUSoADataManager and CPUPreviewSystem
	constexpr int CPU_PREVIEW_MAX_CAPACITY{256};
	
	// GPU Main System: Full-scale simulation supporting up to 10,000 cells
	// Used by CellManager for GPU-accelerated simulation
	constexpr int GPU_MAIN_MAX_CAPACITY{10000};
	
	// Adhesion Configuration (shared between both systems)
	constexpr int MAX_ADHESIONS_PER_CELL{ 20 }; // Maximum number of adhesions per cell
	
	// System-Specific Adhesion Capacities
	// CPU Preview System: Maximum adhesion connections for preview simulation
	constexpr int CPU_PREVIEW_MAX_ADHESIONS{ (CPU_PREVIEW_MAX_CAPACITY * MAX_ADHESIONS_PER_CELL) / 2 };
	
	// GPU Main System: Maximum adhesion connections for full simulation
	constexpr int GPU_MAIN_MAX_ADHESIONS{ (GPU_MAIN_MAX_CAPACITY * MAX_ADHESIONS_PER_CELL) / 2 };
	
	// Other Configuration
	constexpr float DEFAULT_SPAWN_RADIUS{50.0f};
	constexpr int COUNTER_NUMBER{ 4 }; // Number of counters in the cell count buffer

	// ========== Particle System Configuration ==========
	// Particle indices in the unified spatial grid start after all possible cell indices
	// This offset ensures particles and cells can coexist in the same grid without ID conflicts
	constexpr int PARTICLE_SPATIAL_GRID_INDEX_OFFSET{ GPU_MAIN_MAX_CAPACITY };

	// ========== Spatial Partitioning Configuration ==========
	constexpr float WORLD_SIZE{100.0f};                          // Size of the simulation world (cube from -50 to +50)
	constexpr int GRID_RESOLUTION{64};                            // Increased from 32 to 64: 64^3 = 262,144 total grid cells for better distribution
	constexpr float GRID_CELL_SIZE{WORLD_SIZE / GRID_RESOLUTION}; // Size of each grid cell (~1.56 units)
	constexpr int MAX_CELLS_PER_GRID{32};                         // Reduced from 64 to 32: better memory access patterns
	constexpr int TOTAL_GRID_CELLS{GRID_RESOLUTION * GRID_RESOLUTION * GRID_RESOLUTION};
	
	// ========== Sphere Skin Configuration ==========
	constexpr float SPHERE_RADIUS{50.0f};                        // Radius of sphere boundary for culling
	constexpr glm::vec3 SPHERE_CENTER{0.0f, 0.0f, 0.0f};       // Center of sphere (world origin)
	constexpr bool ENABLE_SPHERE_CULLING{true};                  // Enable sphere-based culling
	constexpr bool ENABLE_SPHERE_SKIN_VISUALIZATION{true};       // Enable visual sphere skin rendering
	constexpr glm::vec3 SPHERE_SKIN_COLOR{0.2f, 0.4f, 0.8f};   // Color of sphere skin (light blue)
	constexpr float SPHERE_SKIN_TRANSPARENCY{0.99f};             // Transparency of sphere skin (0.0 = invisible, 1.0 = opaque)
	
	// ========== Velocity Barrier Configuration ==========
	constexpr bool ENABLE_VELOCITY_BARRIER{true};                // Enable velocity reversal barrier at sphere boundary
	constexpr float BARRIER_DAMPING{0.8f};                       // Damping factor when velocity is reversed (0.0 = full stop, 1.0 = no damping)
	constexpr float BARRIER_PUSH_DISTANCE{2.0f};                 // Distance inside boundary to push cells back when they breach

	// ========== Rendering Configuration ==========
	// Distance-based culling and fading parameters
	constexpr float defaultMaxRenderDistance{170.0f};         // Maximum distance to render cells
	constexpr float defaultFadeStartDistance{30.0f};          // Distance where fading begins
	constexpr float defaultFadeEndDistance{160.0f};           // Distance where fading ends (cells become invisible)
	constexpr glm::vec3 defaultFogColor{0.0f, 0.0f, 0.0f};    // Atmospheric/fog color for distant cells (completely black)

	// Frustum culling configuration
	constexpr bool defaultUseFrustumCulling{true};            // Enable/disable frustum culling by default
	constexpr float defaultFrustumFov{45.0f};                 // Field of view for frustum calculation
	constexpr float defaultFrustumNearPlane{0.1f};            // Near plane distance for frustum
	constexpr float defaultFrustumFarPlane{1000.0f};          // Far plane distance for frustum

	// LOD (Level of Detail) configuration
	constexpr bool defaultUseLodSystem{true};                 // Enable/disable LOD system by default
	constexpr float defaultLodDistance0{40.0f};               // Distance threshold for LOD level 0 (highest quality)
	constexpr float defaultLodDistance1{80.0f};               // Distance threshold for LOD level 1
	constexpr float defaultLodDistance2{120.0f};              // Distance threshold for LOD level 2
	constexpr float defaultLodDistance3{160.0f};              // Distance threshold for LOD level 3 (lowest quality)

	// Distance culling configuration
	constexpr bool defaultUseDistanceCulling{true};           // Enable/disable distance-based culling by default
	constexpr bool defaultUseDistanceFade{true};              // Enable/disable distance-based fading by default

	// ========== Runtime Configuration Variables ==========
	// These can be modified at runtime
	inline bool showDemoWindow{true};
	inline bool showCircularSliderDemo{true};
	inline float physicsTimeStep{ 0.01f };	// The size of a physics time step for live simulation (accurate physics)
	inline float fastForwardTimeStep{ 0.1f };	// DEPRECATED: Previously used for fast-forward
	inline float resimulationTimeStep{ 0.02f };	// Time step for resimulation (doubled for faster simulation)
	//inline float physicsSpeed{ 1.f };		// A multiplier on the physics tickrate. Physics tickrate = physicsSpeed / physicsTimeStep
	inline float scrubTimeStep{ 0.1f };	// DEPRECATED: Previously used for time scrubber, now uses fastForwardTimeStep
	inline float maxAccumulatorTime{ 0.1f };// Maximum amount of time spent on simulating physics per frame. Max physics tpf = maxAccumulatorTime * tickrate
	inline float maxDeltaTime{ 0.1f };		// The maximum amount of time that can be accumulated by 1 frame

	// ========== CPU Physics Optimization Configuration ==========
	inline bool useMultithreadedCollisions{ true };	// Enable multi-threaded collision detection for CPU physics
	inline int collisionThreadCount{ 4 };			// Number of threads to use for collision detection (0 = auto-detect)
}
