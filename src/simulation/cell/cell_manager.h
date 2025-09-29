#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <map>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include <cstddef> // for offsetof
#include <cstdint> // for uint32_t

#include "../../rendering/core/shader_class.h"
#include "../../input/input.h"
#include "../../core/config.h"
#include "../../rendering/core/mesh/sphere_mesh.h"
#include "../cell/common_structs.h"
#include "../../rendering/systems/frustum_culling.h"

// Forward declaration
class Camera;

// Ensure struct alignment is correct for GPU usage
static_assert(sizeof(ComputeCell) % 16 == 0, "ComputeCell must be 16-byte aligned for GPU usage");
static_assert(sizeof(GPUMode) % 16 == 0, "GPUMode must be 16-byte aligned for GPU usage");
static_assert(sizeof(AdhesionConnection) % 16 == 0, "AdhesionConnection must be 16-byte aligned for GPU usage");

// Adhesion line vertex structure for rendering
struct AdhesionLineVertex {
    glm::vec4 position;    // World position (w unused, but needed for alignment)
    glm::vec4 color;       // RGB color (a unused, but needed for alignment)
};
static_assert(sizeof(AdhesionLineVertex) % 16 == 0, "AdhesionLineVertex must be 16-byte aligned for GPU usage");

struct CellManager
{
    // GPU-based cell management using compute shaders
    // This replaces the CPU-based vectors with GPU buffer objects
    // The compute shaders handle physics calculations and position updates

    // GPU buffer objects - Triple buffered for performance
    GLuint cellBuffer[3]{};         // SSBO for compute cell data (double buffered)
    GLuint instanceBuffer{};        // VBO for instance rendering data
    int bufferRotation{};

    // Cell count management
    GLuint gpuCellCountBuffer{};     // GPU-accessible cell count buffer
    GLuint stagingCellCountBuffer{}; // CPU-accessible cell count buffer (no sync stalls)
    GLuint cellAdditionBuffer{};     // Cell addition queue for GPU
    GLuint uniqueIdBuffer{};         // GPU-accessible unique ID counter buffer

	GLuint freeCellSlotBuffer{}; // Buffer for tracking free slots in the cell buffer
    GLuint freeAdhesionSlotBuffer{}; // Buffer for tracking free slots in the adhesion buffer

    // Cell data staging buffer for CPU reads (avoids GPU->CPU transfer warnings)
    GLuint stagingCellBuffer{};      // CPU-accessible cell data buffer
    void* mappedCellPtr = nullptr;   // Pointer to the cell data staging buffer

    // Genome buffer (immutable, no need for double buffering)
    // It might be a good idea in the future to switch from a flattened mode array to genome structs that contain their own mode arrays
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

    // LOD system
    Shader* lodVertexShader = nullptr;        // Vertex shader for LOD rendering
    Shader* lodComputeShader = nullptr;       // Compute shader for LOD assignment
    GLuint lodInstanceBuffers[4]{};           // Instance buffers for each LOD level
    GLuint lodCountBuffer{};                  // Buffer to track instance counts per LOD level
    int lodInstanceCounts[4]{};               // CPU-side copy of LOD instance counts
    float lodDistances[4] = {
        config::defaultLodDistance0,
        config::defaultLodDistance1,
        config::defaultLodDistance2,
        config::defaultLodDistance3
    }; // Distance thresholds for LOD levels
    bool useLODSystem = config::defaultUseLodSystem;          // Enable/disable LOD system
    
    // LOD instance buffers - separate buffer for each LOD level
    
    // Unified culling system
    Shader* unifiedCullShader = nullptr;      // Unified compute shader for all culling modes
    Shader* distanceFadeShader = nullptr;     // Vertex/fragment shaders for distance-based fading
    GLuint unifiedOutputBuffers[4]{};         // Output buffers for each LOD level
    GLuint unifiedCountBuffer{};              // Buffer for LOD counts
    bool useFrustumCulling = config::defaultUseFrustumCulling;            // Enable/disable frustum culling
    bool useDistanceCulling = config::defaultUseDistanceCulling;          // Enable/disable distance-based culling
    Frustum currentFrustum;                   // Current camera frustum
    int visibleCellCount{0};                  // Number of visible cells after culling
    float maxRenderDistance = config::defaultMaxRenderDistance;         // Maximum distance to render cells
    float fadeStartDistance = config::defaultFadeStartDistance;          // Distance where fading begins
    float fadeEndDistance = config::defaultFadeEndDistance;              // Distance where fading ends
    
    // LOD statistics functions
    int getTotalTriangleCount() const;        // Calculate total triangles across all LOD levels
    int getTotalVertexCount() const;          // Calculate total vertices across all LOD levels
    
    // Cached statistics for performance (updated when LOD counts change)
    mutable int cachedTriangleCount = -1;     // -1 means invalid cache
    mutable int cachedVertexCount = -1;       // -1 means invalid cache
    
    // Helper to invalidate cache when needed
    void invalidateStatisticsCache() const {
        cachedTriangleCount = -1;
        cachedVertexCount = -1;
    }
    
    // Distance culling parameters
    glm::vec3 fogColor = config::defaultFogColor; // Atmospheric/fog color for distant cells

    // Compute shaders
    Shader* physicsShader = nullptr;
    Shader* positionUpdateShader = nullptr;
    Shader* velocityUpdateShader = nullptr;
    Shader* extractShader = nullptr; // For extracting instance data efficiently
    Shader* internalUpdateShader = nullptr;
    Shader* cellAdditionShader = nullptr;

    // Spatial partitioning compute shaders
    Shader* gridClearShader = nullptr;     // Clear grid counts
    Shader* gridAssignShader = nullptr;    // Assign cells to grid
    Shader* gridPrefixSumShader = nullptr; // Calculate grid offsets
    Shader* gridInsertShader = nullptr;    // Insert cells into grid
    
    // CPU-side storage for initialization and debugging
    // Note: cpuCells is deprecated in favor of GPU buffers, should be removed after refactoring
    std::vector<ComputeCell> cpuCells;
    std::vector<ComputeCell> cellStagingBuffer;
    
    // Cell count tracking (CPU-side approximation of GPU state)
    int totalCellCount{ 0 };    // Approximate cell count, may not reflect exact GPU state due to being a frame behind
	int liveCellCount{ 0 };     // Number of live cells (not dead or pending)
	int totalAdhesionCount{ 0 };     // Total number of adhesion connections
    int liveAdhesionCount{ 0 };     // Number of live adhesion connections
    int pendingCellCount{ 0 };  // Number of cells pending addition by CPU
    
    // Lineage tracking
    uint32_t nextUniqueId{ 1 }; // Next unique ID to assign (starts at 1, 0 reserved for root)
    void* mappedPtr = nullptr;      // Pointer to the cell count staging buffer
    GLuint* countPtr = nullptr;     // Typed pointer to the mapped buffer value
    void syncCounterBuffers()
    {
        glCopyNamedBufferSubData(gpuCellCountBuffer, stagingCellCountBuffer, 0, 0, sizeof(GLuint) * config::COUNTER_NUMBER);
    }
    void updateCounts();

    // Configuration
    static constexpr int DEFAULT_CELL_COUNT = config::DEFAULT_CELL_COUNT;
    float spawnRadius = config::DEFAULT_SPAWN_RADIUS;
    int cellLimit = config::MAX_CELLS;
	int getAdhesionLimit() const { return cellLimit * config::MAX_ADHESIONS_PER_CELL / 2; } // Maximum number of adhesion connections

    // Constructor and destructor
    CellManager();
    ~CellManager();

    // We declare functions in the struct, but we will define them in the cell_manager.cpp file.
    // This is because when a file is edited, the compiler will also have to recompile all the files that include it.
    // So we will define the functions in a separate file to avoid recompiling the whole project when we change the implementation.

    void initializeGPUBuffers();
    void resetSimulation();
    void spawnCells(int count = DEFAULT_CELL_COUNT);
    void renderCells(glm::vec2 resolution, Shader &cellShader, class Camera &camera, bool wireframe = false);
    // Gizmo orientation visualization
    GLuint gizmoBuffer{};           // Buffer for gizmo line vertices
    GLuint gizmoVAO{};              // VAO for gizmo rendering
    GLuint gizmoVBO{};              // VBO for gizmo vertices
    Shader* gizmoExtractShader = nullptr; // Compute shader for generating gizmo data
    Shader* gizmoShader = nullptr;        // Vertex/fragment shaders for rendering gizmos
    
    // Ring gizmo visualization
    GLuint ringGizmoBuffer{};           // Buffer for ring gizmo vertices
    GLuint ringGizmoVAO{};              // VAO for ring gizmo rendering
    GLuint ringGizmoVBO{};              // VBO for ring gizmo vertices
    Shader* ringGizmoExtractShader = nullptr; // Compute shader for generating ring gizmo data
    Shader* ringGizmoShader = nullptr;        // Vertex/fragment shaders for rendering ring gizmos
    
    // Anchor gizmo visualization
    GLuint anchorGizmoBuffer{};           // Buffer for anchor gizmo instances
    GLuint anchorGizmoVBO{};              // VBO for anchor gizmo instances
    GLuint anchorCountBuffer{};           // Buffer for anchor count
    uint32_t totalAnchorCount{};              // Total number of anchor instances
    Shader* anchorGizmoExtractShader = nullptr; // Compute shader for generating anchor gizmo data
    Shader* anchorGizmoShader = nullptr;        // Vertex/fragment shaders for rendering anchor gizmos
    
    // Adhesion line visualization
    GLuint adhesionLineBuffer{};        // Buffer for adhesionSettings line vertices  
    GLuint adhesionLineVAO{};           // VAO for adhesionSettings line rendering
    GLuint adhesionLineVBO{};           // VBO for adhesionSettings line vertices
    Shader* adhesionLineExtractShader = nullptr; // Generate adhesionSettings line data
    Shader* adhesionLineShader = nullptr;        // Vertex/fragment shaders for rendering adhesionSettings lines

    // Adhesion connection system
    GLuint adhesionConnectionBuffer{};  // Buffer storing permanent adhesionSettings connections
    Shader* adhesionPhysicsShader = nullptr;  // Compute shader for processing adhesionSettings physics

    // ============================================================================
    // ENHANCED DIAGNOSTIC SYSTEM
    // ============================================================================
    
    // Event Type Categories
    enum class DiagnosticEventType : uint32_t {
        // Adhesion Events (0-29)
        ADHESION_INHERITED = 0,        // Child inherits parent's connection during split
        ADHESION_DIRECT = 1,           // Normal new connection between cells
        ADHESION_BROKEN = 2,           // Connection broken due to force/distance
        ADHESION_SPLIT_EVENT = 3,      // Original connection removed due to parent split
        ADHESION_CELL_DEATH = 4,       // Connection removed due to cell death
        ADHESION_FORCE_BREAK = 5,      // Connection broken by excessive force
        ADHESION_DISTANCE_BREAK = 6,   // Connection broken by excessive distance
        ADHESION_MODE_CHANGE = 7,      // Connection removed due to mode change
        ADHESION_CAPACITY_FULL = 8,    // Connection failed due to cell adhesion capacity
        ADHESION_INVALID_CELLS = 9,    // Connection failed due to invalid cell indices
        ADHESION_DUPLICATE = 10,       // Connection failed due to duplicate connection
        ADHESION_SELF_CONNECTION = 11, // Connection failed due to self-connection attempt
        ADHESION_OUT_OF_BOUNDS = 12,   // Connection failed due to out of bounds
        ADHESION_LIMIT = 13,           // Connection failed due to global adhesion limit
        ADHESION_RESTORE = 14,         // Connection restored from keyframe/save
        ADHESION_MANUAL_REMOVE = 15,   // Connection manually removed by user
        ADHESION_COLLISION_BREAK = 16, // Connection broken due to collision
        ADHESION_AGE_BREAK = 17,       // Connection broken due to cell age
        ADHESION_TOXIN_BREAK = 18,     // Connection broken due to high toxins
        ADHESION_SIGNALING_BREAK = 19, // Connection broken due to signaling
        ADHESION_UNKNOWN_ERROR = 20,   // Connection failed for unknown reason
        
        // Cell Lifecycle Events (30-59)
        CELL_BIRTH = 30,               // New cell created
        CELL_DEATH = 31,               // Cell died
        CELL_SPLIT_START = 32,         // Cell begins splitting process
        CELL_SPLIT_COMPLETE = 33,      // Cell split completed
        CELL_REPLACED = 34,            // Parent cell replaced by children during split
        CELL_MODE_CHANGE = 35,         // Cell changed mode
        CELL_GENOME_MUTATION = 36,     // Cell genome mutated
        CELL_COLLISION = 37,           // Cell collision detected
        CELL_OUT_OF_BOUNDS = 38,       // Cell moved out of world bounds
        CELL_SELECTED = 39,            // Cell selected by user
        CELL_DRAGGED = 40,             // Cell being dragged by user
        
        // Physics Events (60-89)
        PHYSICS_HIGH_VELOCITY = 60,    // Cell exceeded velocity threshold
        PHYSICS_HIGH_ACCELERATION = 61, // Cell exceeded acceleration threshold
        PHYSICS_CONSTRAINT_VIOLATION = 62, // Physics constraint violated
        PHYSICS_INSTABILITY = 63,      // Physics instability detected
        
        // System Events (90-99)
        SYSTEM_BUFFER_OVERFLOW = 90,   // Diagnostic buffer overflow
        SYSTEM_PERFORMANCE_WARNING = 91, // Performance threshold exceeded
        SYSTEM_ERROR = 99              // General system error
    };

    // Genome difference tracking structure
    struct GenomeDifference {
        std::string propertyName;
        std::string defaultValue;
        std::string currentValue;
        float numericDifference; // For numeric values, 0 for non-numeric
    };

#pragma pack(push, 4)
    // Enhanced diagnostic entry with genome tracking
    struct EnhancedDiagnosticEntry {
        // Core event data
        uint32_t eventType;           // DiagnosticEventType
        uint32_t frameIndex;          // Frame when event occurred
        uint32_t cellA;               // Primary cell involved (index)
        uint32_t cellB;               // Secondary cell (if applicable, -1 otherwise) (index)
        
        // Lineage tracking data stored directly for accurate historical logging
        
        // Event-specific data
        uint32_t connectionIndex;     // For adhesion events
        uint32_t modeIndex;           // Mode of primary cell
        uint32_t splitEventID;        // Groups related split events
        uint32_t parentCellID;        // For inheritance tracking
        
        // Spatial data
        float positionA[3];           // Position of cell A
        float positionB[3];           // Position of cell B (if applicable)
        float anchorDirA[3];          // Anchor direction for cell A
        float anchorDirB[3];          // Anchor direction for cell B
        
        // Physics data
        float velocityA[3];           // Velocity of cell A
        float velocityB[3];           // Velocity of cell B (if applicable)
        float massA;                  // Mass of cell A
        float massB;                  // Mass of cell B (if applicable)
        
        // Cell state data
        float ageA;                   // Age of cell A
        float ageB;                   // Age of cell B (if applicable)
        float toxinsA;                // Toxin level of cell A
        float toxinsB;                // Toxin level of cell B (if applicable)
        float nitratesA;              // Nitrate level of cell A
        float nitratesB;              // Nitrate level of cell B (if applicable)
        
        // Signaling substances
        float signallingA[4];         // Signalling substances of cell A
        float signallingB[4];         // Signalling substances of cell B
        
        // Lineage data for cell A
        uint32_t lineageA_parentId;   // Parent unique ID for cell A
        uint32_t lineageA_uniqueId;   // Unique ID for cell A
        uint32_t lineageA_childNum;   // Child number for cell A

        // Lineage data for cell B
        uint32_t lineageB_parentId;   // Parent unique ID for cell B
        uint32_t lineageB_uniqueId;   // Unique ID for cell B
        uint32_t lineageB_childNum;   // Child number for cell B

        // Additional event data
        float eventValue1;            // Generic event-specific value 1
        float eventValue2;            // Generic event-specific value 2
        uint32_t eventFlags;          // Bit flags for event properties

        // Padding to ensure 16-byte alignment (added 6 uint32_t fields = 24 bytes)
        float _padding[1];
    };
#pragma pack(pop)
    static_assert(sizeof(EnhancedDiagnosticEntry) % 16 == 0, "EnhancedDiagnosticEntry must be 16-byte aligned");

    // Diagnostic system state
    struct DiagnosticSystemState {
        bool adhesionEventsEnabled = true;
        bool cellLifecycleEventsEnabled = true;
        bool physicsEventsEnabled = false;
        bool systemEventsEnabled = true;
        bool genomeTrackingEnabled = true;
        bool lineageTrackingEnabled = true;
        bool realTimeMonitoringEnabled = false;
        
        // Performance thresholds
        float velocityThreshold = 50.0f;
        float accelerationThreshold = 100.0f;
        float toxinThreshold = 0.8f;
        
        // Buffer management
        uint32_t maxEntries = 100000;
        uint32_t currentEntries = 0;
        bool bufferOverflowOccurred = false;
    };

    // Diagnostic buffers and state
    GLuint enhancedDiagnosticBuffer{};
    GLuint diagnosticCountBuffer{};
    DiagnosticSystemState diagnosticState{};
    bool diagnosticsRunning{false};
    
    // Genome tracking
    std::unordered_map<uint32_t, std::vector<GenomeDifference>> cellGenomeDifferences;
    ModeSettings defaultModeSettings; // Reference for comparison
    GenomeData currentGenome; // Current genome data
    uint32_t currentFrame = 0; // Current simulation frame
    
    // Lineage tracking data
    std::unordered_map<uint32_t, uint32_t> cellIdToIndex; // Map unique ID to cell index
    std::unordered_map<uint32_t, std::vector<uint32_t>> lineageChildren; // Parent ID -> children IDs
    std::unordered_map<uint32_t, uint32_t> lineageParents; // Child ID -> parent ID

    // Enhanced diagnostic methods
    void initializeEnhancedDiagnostics();
    void toggleEnhancedDiagnostics();
    void logDiagnosticEvent(DiagnosticEventType eventType, uint32_t cellA, uint32_t cellB = UINT32_MAX, 
                           uint32_t connectionIndex = UINT32_MAX, float eventValue1 = 0.0f, float eventValue2 = 0.0f);
    void updateGenomeTracking(uint32_t cellIndex, const ModeSettings& currentMode);
    void writeEnhancedDiagnosticLog();
    void clearDiagnosticData();
    std::vector<GenomeDifference> calculateGenomeDifferences(const ModeSettings& current, const ModeSettings& defaultMode);
    
    // Lineage tracking methods
    void updateLineageTracking(uint32_t cellIndex);
    void syncLineageTrackingFromGPU();
    std::unordered_map<uint32_t, std::vector<uint32_t>> getLineageTree() const;
    std::vector<uint32_t> getLineageAncestors(uint32_t cellId) const;
    std::vector<uint32_t> getLineageDescendants(uint32_t cellId) const;
    uint32_t getLineageDepth(uint32_t cellId) const;
    std::string getLineageStatistics() const;
    
    // Legacy adhesion diagnostic methods (for backward compatibility)
    void initializeAdhesionDiagnostics() { initializeEnhancedDiagnostics(); }
    void toggleAdhesionDiagnostics() { toggleEnhancedDiagnostics(); }
    void writeAdhesionDiagnosticLog(); // Implementation in cpp file
    
    // Real-time monitoring
    void updateRealTimeMonitoring();
    void syncDiagnosticCounter();
    std::vector<std::string> getRecentEvents(int maxEvents = 50);
    
    // Performance monitoring
    void checkPerformanceThresholds();
    
    // Legacy compatibility flags
    bool adhesionDiagnosticsRunning{false}; // Kept for compatibility, redirects to enhanced system
    
    void initializeGizmoBuffers();
    void updateGizmoData();
    void cleanupGizmos();
    void renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos);
    
    // Ring gizmo methods
    void renderRingGizmos(glm::vec2 resolution, const class Camera &camera, const class UIManager &uiManager);
    void renderAnchorGizmos(glm::vec2 resolution, const class Camera &camera, const class UIManager &uiManager);
    void initializeRingGizmoBuffers();
    void updateRingGizmoData();
    void cleanupRingGizmos();
    
    // Anchor gizmo methods
    void initializeAnchorGizmoBuffers();
    void updateAnchorGizmoData();
    void cleanupAnchorGizmos();
    
    // Adhesion line methods
    void renderAdhesionLines(glm::vec2 resolution, const class Camera &camera, bool showAdhesionLines);
    void initializeAdhesionLineBuffers();
    void updateAdhesionLineData();
    void cleanupAdhesionLines();

    void initializeAdhesionConnectionSystem();
    void cleanupAdhesionConnectionSystem();

    // CELL ADDITION RULES:
	// Add cells to the staging buffer, which is then processed by the GPU automatically every frame.
	// Do not add cells directly to the GPU buffer, as they may not be processed immediately, and may be overwritten. Use the staging buffer instead.
    void addCellsToQueueBuffer(const std::vector<ComputeCell> &cells);
    void addCellToStagingBuffer(const ComputeCell &newCell);
    void addStagedCellsToQueueBuffer();
    void addGenomeToBuffer(GenomeData& genomeData) const;
    void updateCells(float deltaTime);
    void cleanup();

    // Spatial partitioning functions
    void initializeSpatialGrid();
    void updateSpatialGrid();
    void cleanupSpatialGrid();

    // Getter functions for debug information
    int getCellCount() const { return totalCellCount; }
    float getSpawnRadius() const { return spawnRadius; }

    // Performance testing function
	// Cell selection and interaction system
    struct SelectedCellInfo
    {
        int cellIndex = -1;
        ComputeCell cellData;
        bool isValid = false;
        glm::vec3 dragOffset = glm::vec3(0.0f); // Offset from cell center when dragging starts
        float dragDistance = 10.0f;             // Distance from camera to maintain during dragging
    };

    SelectedCellInfo selectedCell;
    bool isDraggingCell = false;

    // Selection and interaction functions
    void handleMouseInput(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                          const class Camera &camera, bool isMousePressed, bool isMouseDown,
                          float scrollDelta = 0.0f);
    int selectCellAtPosition(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection);
    void dragSelectedCell(const glm::vec3 &newWorldPosition);
    void clearSelection();

    // Handle the end of dragging (restore physics)
    void endDrag();

    // GPU synchronization for selection (synchronous readback for immediate use)
    void syncCellPositionsFromGPU();

    // Utility functions for mouse interaction
    glm::vec3 calculateMouseRay(const glm::vec2 &mousePos, const glm::vec2 &screenSize,
                                const class Camera &camera);
    bool raySphereIntersection(const glm::vec3 &rayOrigin, const glm::vec3 &rayDirection,
                               const glm::vec3 &sphereCenter, float sphereRadius, float &distance);

    // Getters for selection system
    bool hasSelectedCell() const { return selectedCell.isValid; }
    const SelectedCellInfo &getSelectedCell() const { return selectedCell; }
    ComputeCell getCellData(int index) const;
    uint32_t getCurrentFrame() const { return currentFrame; }
    uint32_t getCurrentCellCount() const { return static_cast<uint32_t>(totalCellCount); }
    const GenomeData& getGenomeData() const { return currentGenome; }
    void updateCellData(int index, const ComputeCell &newData); // Needs refactoring
    
    // Lineage tracking helpers
    std::string getCellLineageString(int index) const;
    uint32_t getNextUniqueId() const { return nextUniqueId; }

    // Memory barrier optimization system
    // Forward declaration and performance monitoring
    struct BarrierStats {
        int totalBarriers = 0;
        int batchedBarriers = 0;
        int flushCalls = 0;
        float barrierEfficiency = 0.0f; // batchedBarriers / totalBarriers
        
        void reset() {
            totalBarriers = 0;
            batchedBarriers = 0;
            flushCalls = 0;
            barrierEfficiency = 0.0f;
        }
        
        void updateEfficiency() {
            if (totalBarriers > 0) {
                barrierEfficiency = static_cast<float>(batchedBarriers) / totalBarriers;
            }
        }
    };
    
    struct BarrierBatch {
        GLbitfield pendingBarriers = 0;
        bool needsFlush = false;
        mutable BarrierStats* stats = nullptr; // Reference to stats for tracking
        
        void addBarrier(GLbitfield barrier) {
            pendingBarriers |= barrier;
            if (stats) {
                stats->totalBarriers++;
                if (pendingBarriers != barrier) {
                    stats->batchedBarriers++; // This barrier was batched with others
                }
            }
        }
        
        void flush() {
            if (pendingBarriers != 0) {
                glMemoryBarrier(pendingBarriers);
                pendingBarriers = 0;
                if (stats) {
                    stats->flushCalls++;
                    stats->updateEfficiency();
                }
            }
            needsFlush = false;
        }
        
        void clear() {
            pendingBarriers = 0;
            needsFlush = false;
        }
        
        void setStats(BarrierStats* statsPtr) {
            stats = statsPtr;
        }
    };
    
    mutable BarrierBatch barrierBatch;
    mutable BarrierStats barrierStats;
    
    // Optimized barrier methods
    void addBarrier(GLbitfield barrier) const { barrierBatch.addBarrier(barrier); }
    void flushBarriers() const { barrierBatch.flush(); }
    void clearBarriers() const { barrierBatch.clear(); }
    
    // Debug methods for barrier optimization
    const BarrierStats& getBarrierStats() const { return barrierStats; }
    void resetBarrierStats() const { barrierStats.reset(); }

    // Triple buffering management functions
    int getRotatedIndex(int index, int max) const { return (index + bufferRotation) % max; }
    void rotateBuffers() { bufferRotation = getRotatedIndex(1, 3); }
    GLuint getCellReadBuffer() const { return cellBuffer[getRotatedIndex(0, 3)]; }
    GLuint getCellWriteBuffer() const { return cellBuffer[getRotatedIndex(1, 3)]; }
    // BUFFER ACCESS RULES:
	// NEVER write to the read buffer directly, because that will be overwritten by the next shader pass
    // Read from the read buffer and write to the write buffer
	// Rotate buffers after each shader pass that writes to them to ensure correct read/write access
	// Do not rotate buffers when you don't need to write to them; this will undo the previous shader pass
	// All threads write to the write buffer, even if some don't have new data to write. This is to ensure that the write buffer is always fully updated.

    // Minor exception:
	// If each thread reads and writes to and from the same single cell, and only that cell, then you can use the read buffer for both reading and writing.
	// This is useful for operations that only affect a single cell, like dragging or selection.
	// This is fine for operations that occur independently of other cells, like updating cells' positions.
	// Do not rotate the buffers after this operation, as it will undo the previous shader pass.
	// If you are unsure if you can use this exception, do not use it.
	// If there are weird bugs when using this exception, try not using it and see if the bugs go away.

    // Frame |  Write   | Read | Standby
    //     1 |    B0    |  B1  |   B2
    //     2 |    B2    |  B0  |   B1
    //     3 |    B1    |  B2  |   B0

	// Frame | B0 | B1 | B2
	//     1 |  W |  R |  S
	//     2 |  R |  S |  W
	//     3 |  S |  W |  R

    void setCellLimit(int limit) { cellLimit = limit; }
    int getCellLimit() const { return cellLimit; }
    
    // LOD system functions
    void initializeLODSystem();
    void cleanupLODSystem();
    void updateLODLevels(const Camera& camera);
    void renderCellsLOD(glm::vec2 resolution, const Camera& camera, bool wireframe = false);
    void runLODCompute(const Camera& camera);
    
    // Unified culling functions
    void initializeUnifiedCulling();
    void cleanupUnifiedCulling();
    void updateFrustum(const Camera& camera, float fov, float aspectRatio, float nearPlane, float farPlane);
    void runUnifiedCulling(const Camera& camera);
    void renderCellsUnified(glm::vec2 resolution, const Camera& camera, bool wireframe = false);
    void setDistanceCullingParams(float maxDistance, float fadeStart, float fadeEnd);
    int getVisibleCellCount() const { return visibleCellCount; }
    
    // Getter functions for distance culling parameters
    float getMaxRenderDistance() const { return maxRenderDistance; }
    float getFadeStartDistance() const { return fadeStartDistance; }
    float getFadeEndDistance() const { return fadeEndDistance; }
    glm::vec3 getFogColor() const { return fogColor; }
    void setFogColor(const glm::vec3& color) { fogColor = color; }

    void restoreCellsDirectlyToGPUBuffer(const std::vector<ComputeCell> &cells); // For keyframe restoration
    void setCPUCellData(const std::vector<ComputeCell> &cells); // For keyframe restoration
    
    // Adhesion connection methods for keyframe support
    std::vector<AdhesionConnection> getAdhesionConnections() const; // Get current adhesion connections
    void restoreAdhesionConnections(const std::vector<AdhesionConnection> &connections, int count); // Restore adhesion connections

private:
    void applyForces(float deltaTime);
    void verletIntegration(float deltaTime);
    void runCollisionCompute();
    void runPositionUpdateCompute(float deltaTime);
    void runVelocityUpdateCompute(float deltaTime);
    void runInternalUpdateCompute(float deltaTime);
    void runAdhesionPhysics(float deltaTime);
    void applyCellAdditions();

    // Spatial grid helper functions
    void runGridClear();
    void runGridAssign();
    void runGridPrefixSum();
    void runGridInsert();
};
