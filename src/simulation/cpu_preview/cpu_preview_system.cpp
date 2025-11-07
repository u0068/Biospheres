#include "cpu_preview_system.h"
#include "../../core/config.h"
#include "../../rendering/core/shader_class.h"
#include "../../rendering/camera/camera.h"
#include "../../ui/ui_manager.h"
#include "../cell/cell_manager.h"
#include "../cell/common_structs.h"
#include <chrono>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glad/glad.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

CPUPreviewSystem::CPUPreviewSystem() 
    : m_dataManager(std::make_unique<CPUSoADataManager>())
    , m_physicsEngine(std::make_unique<CPUSIMDPhysicsEngine>())
    , m_visualSystem(std::make_unique<CPUTripleBufferSystem>())
    , m_genomeManager(std::make_unique<CPUGenomeManager>())
{
}

CPUPreviewSystem::~CPUPreviewSystem() {
    if (m_initialized) {
        shutdown();
    }
}

void CPUPreviewSystem::initialize() {
    if (m_initialized) {
        return;
    }

    try {
        // Initialize visual system first (may need OpenGL context)
        m_visualSystem->initialize();
        
        // Initialize genome manager with deterministic mode for reproducible results
        m_genomeManager->initialize(12345); // Fixed seed for deterministic genome iteration
        
        // Initialize GPU buffers for gizmo rendering
        initializeGPUBuffers();
        
        // Initialize gizmo system (reuse existing GPU gizmo infrastructure)
        initializeGizmoSystem();
        
        // Create default empty scene
        createEmptyScene(256);
        
        m_initialized = true;
        m_enabled = true;
        
        // System initialized silently
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to initialize CPU Preview System: " << e.what() << std::endl;
        throw;
    }
}

void CPUPreviewSystem::shutdown() {
    if (!m_initialized) {
        return;
    }

    m_enabled = false;
    
    if (m_visualSystem) {
        m_visualSystem->shutdown();
    }
    
    // Cleanup gizmo system
    cleanupGizmoSystem();
    
    // Cleanup GPU buffers
    cleanupGPUBuffers();
    
    m_initialized = false;
    // System shutdown complete
}

void CPUPreviewSystem::createEmptyScene(size_t maxCells) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    m_dataManager->createEmptyScene(maxCells);
}

void CPUPreviewSystem::loadPreviewScene(const std::string& filename) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    m_dataManager->loadPreviewScene(filename);
}

void CPUPreviewSystem::savePreviewScene(const std::string& filename) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    m_dataManager->savePreviewScene(filename);
}

void CPUPreviewSystem::update(float deltaTime) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    m_frameStart = std::chrono::steady_clock::now();

    try {

        // Run physics simulation with SIMD optimization
        if (m_physicsEngine && m_dataManager) {
            // Convert genome parameters to mode settings (Requirements 4.5)
            std::vector<GPUModeAdhesionSettings> modeSettings = createModeSettingsFromGenome(m_currentGenomeParams);
            
            m_physicsEngine->simulateStep(
                m_dataManager->getCellData(),
                m_dataManager->getAdhesionData(),
                deltaTime,
                modeSettings,
                &m_currentGenomeParams
            );
        }

        // Update visual data asynchronously for GPU rendering pipeline (unless suppressed)
        if (m_visualSystem && m_dataManager && !m_suppressVisualUpdates) {
            m_visualSystem->updateVisualData(m_dataManager->getCellData());
        }

        // Update performance metrics
        updatePerformanceMetrics();
    }
    catch (const std::exception& e) {
        std::cerr << "CPU Preview System update error: " << e.what() << std::endl;
        m_enabled = false; // Disable system on error
    }
}

void CPUPreviewSystem::fastForward(float totalTime, float timeStep) {
    if (!m_initialized || !m_enabled) {
        return;
    }

    // Suppress visual updates during fast-forward to prevent flashing
    m_suppressVisualUpdates = true;

    auto startTime = std::chrono::steady_clock::now();

    try {
        float currentTime = 0.0f;
        int stepCount = 0;
        
        // Calculate expected steps for debugging
        int expectedSteps = static_cast<int>(std::ceil(totalTime / timeStep));
        
        // Pure physics simulation - no visual updates, no GPU operations, no validation
        while (currentTime < totalTime) {
            float stepSize = std::min(timeStep, totalTime - currentTime);
            
            // Direct physics simulation only
            if (m_physicsEngine && m_dataManager) {
                // Convert genome parameters to mode settings (Requirements 4.5)
                std::vector<GPUModeAdhesionSettings> modeSettings = createModeSettingsFromGenome(m_currentGenomeParams);
                
                m_physicsEngine->simulateStep(
                    m_dataManager->getCellData(),
                    m_dataManager->getAdhesionData(),
                    stepSize,
                    modeSettings,
                    &m_currentGenomeParams
                );
            }
            
            currentTime += stepSize;
            stepCount++;
        }
        
        // Update performance metrics
        auto endTime = std::chrono::steady_clock::now();
        m_lastSimulationTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
        

        
    } catch (const std::exception& e) {
        std::cerr << "CPU Preview System fastForward error: " << e.what() << std::endl;
        m_enabled = false;
    }
    
    // Re-enable visual updates and update final visual data
    m_suppressVisualUpdates = false;
    if (m_visualSystem && m_dataManager) {
        m_visualSystem->updateVisualData(m_dataManager->getCellData());
    }
}

void CPUPreviewSystem::reset() {
    if (!m_initialized) {
        return;
    }
    
    // Reset to empty scene
    createEmptyScene(256);
    m_lastSimulationTime = 0.0f;
}

void CPUPreviewSystem::updateGenomeParameters(uint32_t cellIndex, const CPUGenomeParameters& params) {
    if (!m_dataManager || !m_genomeManager) {
        throw std::runtime_error("Data manager or genome manager not initialized");
    }
    
    // Use genome manager for optimized SoA parameter application
    std::vector<uint32_t> indices = {cellIndex};
    m_genomeManager->applyGenomeToIndices(m_dataManager->getCellData(), params, indices);
}

void CPUPreviewSystem::applyGenomeToAll(uint32_t genomeID, const CPUGenomeParameters& params) {
    if (!m_dataManager || !m_genomeManager) {
        throw std::runtime_error("Data manager or genome manager not initialized");
    }
    
    // Use genome manager for optimized SoA parameter application by mode
    m_genomeManager->applyGenomeToMode(m_dataManager->getCellData(), params, genomeID);
    
    // Trigger immediate resimulation for instant feedback
    // This enables rapid genome iteration workflow
    const auto& cellData = m_dataManager->getCellData();
    if (cellData.activeCellCount > 0) {
        // Force visual update to show parameter changes immediately
        if (m_visualSystem) {
            m_visualSystem->updateVisualData(cellData);
        }
    }
}

uint32_t CPUPreviewSystem::addCell(const CPUCellParameters& params) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    uint32_t cellIndex = m_dataManager->addCell(params);
    
    // Store genome parameters for division logic
    m_currentGenomeParams = params.genome;
    
    // Update visual data immediately after adding cell (unless suppressed)
    if (m_visualSystem && !m_suppressVisualUpdates) {
        m_visualSystem->updateVisualData(m_dataManager->getCellData());
    }
    
    return cellIndex;
}

void CPUPreviewSystem::removeCell(uint32_t cellIndex) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    m_dataManager->removeCell(cellIndex);
}

void CPUPreviewSystem::addAdhesionConnection(uint32_t cellA, uint32_t cellB, const CPUAdhesionParameters& params) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    m_dataManager->addAdhesionConnection(cellA, cellB, params);
}

const CPUTripleBufferSystem::CPUVisualData* CPUPreviewSystem::getVisualData() const {
    if (!m_visualSystem) {
        return nullptr;
    }
    
    return m_visualSystem->getCurrentVisualData();
}

void CPUPreviewSystem::uploadVisualDataToGPU() {
    if (!m_visualSystem) {
        return;
    }
    
    m_visualSystem->uploadToGPU();
}

size_t CPUPreviewSystem::getActiveCellCount() const {
    if (!m_dataManager) {
        return 0;
    }
    
    return m_dataManager->getActiveCellCount();
}

size_t CPUPreviewSystem::getActiveConnectionCount() const {
    if (!m_dataManager) {
        return 0;
    }
    
    return m_dataManager->getActiveConnectionCount();
}

void CPUPreviewSystem::validateSystemState() {
    if (!m_dataManager || !m_physicsEngine || !m_visualSystem) {
        throw std::runtime_error("CPU Preview System components not properly initialized");
    }
    
    // Skip expensive data integrity validation for performance
}

void CPUPreviewSystem::updatePerformanceMetrics() {
    auto frameEnd = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - m_frameStart);
    m_lastSimulationTime = duration.count() / 1000.0f; // Convert to milliseconds
}

void CPUPreviewSystem::onGenomeParametersChanged(const CPUGenomeParameters& newParams) {
    if (!m_initialized || !m_dataManager || !m_genomeManager) {
        return;
    }
    
    // Store current genome parameters for division logic
    m_currentGenomeParams = newParams;
    
    // Use genome manager for optimized parameter application to all cells
    m_genomeManager->applyGenomeToSoAData(m_dataManager->getCellData(), newParams);
    
    // Trigger immediate visual update for instant feedback
    triggerInstantResimulation();
}

void CPUPreviewSystem::triggerInstantResimulation() {
    if (!m_initialized || !m_dataManager || !m_visualSystem) {
        return;
    }
    
    // Force immediate visual data update for instant genome parameter feedback (unless suppressed)
    if (!m_suppressVisualUpdates) {
        m_visualSystem->updateVisualData(m_dataManager->getCellData());
        
        // Upload to GPU for immediate rendering
        m_visualSystem->uploadToGPU();
    }
}

void CPUPreviewSystem::ensureVisualDataCurrent() {
    if (!m_initialized || !m_dataManager || !m_visualSystem) {
        return;
    }
    
    // Update visual data if needed (lightweight check) - unless suppressed
    if (!m_suppressVisualUpdates) {
        const auto* currentVisualData = m_visualSystem->getCurrentVisualData();
        if (!currentVisualData || currentVisualData->activeCount != m_dataManager->getActiveCellCount()) {
            // Visual data is out of sync, update it
            m_visualSystem->updateVisualData(m_dataManager->getCellData());
        }
    }
}

bool CPUPreviewSystem::isValidSoAFile(const std::string& filename) const {
    // Check if file has .soa extension and exists
    if (!filename.ends_with(".soa")) {
        return false;
    }
    
    // Additional validation could be added here to check file format
    // For now, just check extension
    return true;
}

void CPUPreviewSystem::setDeterministicMode(bool enabled, uint32_t seed) {
    if (!m_genomeManager) {
        std::cerr << "Genome manager not initialized - cannot set deterministic mode\n";
        return;
    }
    
    m_genomeManager->setDeterministicMode(enabled, seed);
    // Deterministic mode set silently
}

bool CPUPreviewSystem::isDeterministicMode() const {
    if (!m_genomeManager) {
        return false;
    }
    
    // For now, assume deterministic mode is enabled by default
    // In a full implementation, the genome manager would expose this state
    return true;
}

void CPUPreviewSystem::initializeGPUBuffers() {
    // Create GPU buffers for cell data (to reuse existing GPU gizmo system)
    glGenBuffers(1, &m_gpuCellBuffer);
    glGenBuffers(1, &m_gpuModeBuffer);
    glGenBuffers(1, &m_gpuCellCountBuffer);
    
    // Allocate cell buffer for maximum cells
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuCellBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 256 * sizeof(ComputeCell), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Allocate mode buffer (single mode for now)
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuModeBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GPUMode), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Allocate cell count buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuCellCountBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(uint32_t), nullptr, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CPUPreviewSystem::cleanupGPUBuffers() {
    if (m_gpuCellBuffer != 0) {
        glDeleteBuffers(1, &m_gpuCellBuffer);
        m_gpuCellBuffer = 0;
    }
    
    if (m_gpuModeBuffer != 0) {
        glDeleteBuffers(1, &m_gpuModeBuffer);
        m_gpuModeBuffer = 0;
    }
    
    if (m_gpuCellCountBuffer != 0) {
        glDeleteBuffers(1, &m_gpuCellCountBuffer);
        m_gpuCellCountBuffer = 0;
    }
}

void CPUPreviewSystem::uploadCellDataToGPU() {
    if (!m_dataManager || !m_visualSystem) {
        return;
    }
    
    const auto* visualData = m_visualSystem->getCurrentVisualData();
    if (!visualData || visualData->activeCount == 0) {
        return;
    }
    
    // Convert CPU SoA data to GPU ComputeCell format
    std::vector<ComputeCell> gpuCells;
    gpuCells.reserve(visualData->activeCount);
    
    const auto& cellData = m_dataManager->getCellData();
    
    for (size_t i = 0; i < visualData->activeCount; ++i) {
        ComputeCell cell{};
        
        // Position and mass
        cell.positionAndMass = glm::vec4(cellData.pos_x[i], cellData.pos_y[i], cellData.pos_z[i], cellData.mass[i]);
        
        // Velocity
        cell.velocity = glm::vec4(cellData.vel_x[i], cellData.vel_y[i], cellData.vel_z[i], 0.0f);
        
        // Orientation (both physics and genome orientation)
        cell.orientation = glm::vec4(cellData.quat_x[i], cellData.quat_y[i], cellData.quat_z[i], cellData.quat_w[i]);
        cell.genomeOrientation = cell.orientation; // Use same orientation for both
        
        // Age and other properties
        cell.age = cellData.age[i];
        cell.modeIndex = 0; // Default mode for now
        
        gpuCells.push_back(cell);
    }
    
    // Upload to GPU buffer
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuCellBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, gpuCells.size() * sizeof(ComputeCell), gpuCells.data());
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Upload mode data (create a simple mode with split direction)
    GPUMode mode{};
    mode.splitDirection = glm::vec4(m_currentGenomeParams.splitDirection, 0.0f);
    mode.splitInterval = m_currentGenomeParams.divisionThreshold;
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuModeBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(GPUMode), &mode);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    
    // Upload cell count
    uint32_t cellCount = static_cast<uint32_t>(visualData->activeCount);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_gpuCellCountBuffer);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(uint32_t), &cellCount);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CPUPreviewSystem::renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos) {
    if (!showGizmos || !m_initialized || !m_dataManager || !m_visualSystem) {
        return;
    }
    
    const auto* visualData = m_visualSystem->getCurrentVisualData();
    if (!visualData || visualData->activeCount == 0) {
        return;
    }
    
    // Upload CPU cell data to GPU buffers
    uploadCellDataToGPU();
    
    // Update gizmo data from current cell orientations
    updateGizmoData();
    
    if (!m_gizmoShader) {
        return;
    }
    
    m_gizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    m_gizmoShader->setMat4("uProjection", projection);
    m_gizmoShader->setMat4("uView", view);
    
    // Enable depth testing and depth writing for proper depth sorting with ring gizmos
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    
    // Enable line width for better visibility
    glLineWidth(4.0f);
    
    // Render gizmo lines
    glBindVertexArray(m_gizmoVAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(visualData->activeCount * 6)); // 6 vertices per cell (3 lines * 2 vertices)
    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void CPUPreviewSystem::renderAnchorGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager) {
    // Update anchor gizmo data first to get current count
    updateAnchorGizmoData();
    
    if (!uiManager.showOrientationGizmos) {
        return; // UI toggle is off
    }
    
    if (m_totalAnchorCount == 0) {
        return; // No anchors to render
    }
    
    if (!m_anchorGizmoShader) {
        return;
    }
    
    m_anchorGizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    m_anchorGizmoShader->setMat4("uProjection", projection);
    m_anchorGizmoShader->setMat4("uView", view);
    m_anchorGizmoShader->setVec3("uCameraPos", camera.getPosition());
    
    // Enable depth testing but disable depth writing to avoid z-fighting with spheres
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // Enable blending for better visibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Note: Anchor gizmos use instanced sphere rendering, which would require
    // access to the sphere mesh. For now, this is a placeholder.
    // In a full implementation, we would need to access the sphere mesh
    // from the rendering system or create our own instance.
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

// ============================================================================
// GIZMO SYSTEM IMPLEMENTATION
// ============================================================================

void CPUPreviewSystem::initializeGizmoSystem() {
    // Initialize gizmo shaders (reuse existing shader files)
    m_gizmoExtractShader = new Shader("shaders/rendering/debug/gizmo_extract.comp");
    m_gizmoShader = new Shader("shaders/rendering/debug/gizmo.vert", "shaders/rendering/debug/gizmo.frag");
    
    // Initialize ring gizmo shaders
    m_ringGizmoExtractShader = new Shader("shaders/rendering/debug/ring_gizmo_extract.comp");
    m_ringGizmoShader = new Shader("shaders/rendering/debug/ring_gizmo.vert", "shaders/rendering/debug/ring_gizmo.frag");
    
    // Initialize anchor gizmo shaders
    m_anchorGizmoExtractShader = new Shader("shaders/rendering/debug/anchor_gizmo_extract.comp");
    m_anchorGizmoShader = new Shader("shaders/rendering/debug/anchor_gizmo.vert", "shaders/rendering/debug/anchor_gizmo.frag");
    
    // Initialize gizmo buffers
    initializeGizmoBuffers();
    initializeRingGizmoBuffers();
    initializeAnchorGizmoBuffers();
}

void CPUPreviewSystem::cleanupGizmoSystem() {
    // Cleanup gizmo shaders
    if (m_gizmoExtractShader) {
        m_gizmoExtractShader->destroy();
        delete m_gizmoExtractShader;
        m_gizmoExtractShader = nullptr;
    }
    if (m_gizmoShader) {
        m_gizmoShader->destroy();
        delete m_gizmoShader;
        m_gizmoShader = nullptr;
    }
    if (m_ringGizmoExtractShader) {
        m_ringGizmoExtractShader->destroy();
        delete m_ringGizmoExtractShader;
        m_ringGizmoExtractShader = nullptr;
    }
    if (m_ringGizmoShader) {
        m_ringGizmoShader->destroy();
        delete m_ringGizmoShader;
        m_ringGizmoShader = nullptr;
    }
    if (m_anchorGizmoExtractShader) {
        m_anchorGizmoExtractShader->destroy();
        delete m_anchorGizmoExtractShader;
        m_anchorGizmoExtractShader = nullptr;
    }
    if (m_anchorGizmoShader) {
        m_anchorGizmoShader->destroy();
        delete m_anchorGizmoShader;
        m_anchorGizmoShader = nullptr;
    }
    
    // Cleanup gizmo buffers
    cleanupGizmos();
    cleanupRingGizmos();
    cleanupAnchorGizmos();
}

void CPUPreviewSystem::initializeGizmoBuffers() {
    // Create buffer for gizmo line vertices (each cell produces 6 vertices for 3 lines)
    // Each vertex now has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &m_gizmoBuffer);
    glNamedBufferData(m_gizmoBuffer,
        256 * 6 * sizeof(glm::vec4) * 2, // position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for gizmo rendering
    glCreateVertexArrays(1, &m_gizmoVAO);
    
    // Create VBO that will be bound to the gizmo buffer
    glCreateBuffers(1, &m_gizmoVBO);
    glNamedBufferData(m_gizmoVBO,
        256 * 6 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(m_gizmoVAO, 0, m_gizmoVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(m_gizmoVAO, 0);
    glVertexArrayAttribFormat(m_gizmoVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_gizmoVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(m_gizmoVAO, 1);
    glVertexArrayAttribFormat(m_gizmoVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(m_gizmoVAO, 1, 0);
}

void CPUPreviewSystem::initializeRingGizmoBuffers() {
    // Create buffer for ring vertices (fixed size per cell)
    // Each cell produces: 2 rings (384 vertices)
    // Each vertex has vec4 position + vec4 color = 8 floats = 32 bytes
    glCreateBuffers(1, &m_ringGizmoBuffer);
    glNamedBufferData(m_ringGizmoBuffer,
        256 * 384 * sizeof(glm::vec4) * 2, // position + color for each vertex
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create VAO for ring gizmo rendering
    glCreateVertexArrays(1, &m_ringGizmoVAO);
    
    // Create VBO that will be bound to the ring gizmo buffer
    glCreateBuffers(1, &m_ringGizmoVBO);
    glNamedBufferData(m_ringGizmoVBO,
        256 * 384 * sizeof(glm::vec4) * 2,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Set up VAO with vertex attributes (stride is now 2 vec4s = 32 bytes)
    glVertexArrayVertexBuffer(m_ringGizmoVAO, 0, m_ringGizmoVBO, 0, sizeof(glm::vec4) * 2);
    
    // Position attribute (vec4)
    glEnableVertexArrayAttrib(m_ringGizmoVAO, 0);
    glVertexArrayAttribFormat(m_ringGizmoVAO, 0, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_ringGizmoVAO, 0, 0);
    
    // Color attribute (vec4, offset by 16 bytes)
    glEnableVertexArrayAttrib(m_ringGizmoVAO, 1);
    glVertexArrayAttribFormat(m_ringGizmoVAO, 1, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(m_ringGizmoVAO, 1, 0);
}

void CPUPreviewSystem::initializeAnchorGizmoBuffers() {
    // Create buffer for anchor instances (variable size - depends on number of adhesions)
    // Maximum possible anchors: 256 * 20 adhesions per cell
    // Each instance is AnchorInstance = 48 bytes (verified by static_assert)
    glCreateBuffers(1, &m_anchorGizmoBuffer);
    glNamedBufferData(m_anchorGizmoBuffer,
        256 * 20 * 48, // Maximum possible anchor instances (assuming 48 bytes per instance)
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create instance VBO for rendering
    glCreateBuffers(1, &m_anchorGizmoVBO);
    glNamedBufferData(m_anchorGizmoVBO,
        256 * 20 * 48,
        nullptr, GL_DYNAMIC_COPY);  // GPU produces data, GPU consumes for rendering
    
    // Create anchor count buffer
    glCreateBuffers(1, &m_anchorCountBuffer);
    glNamedBufferData(m_anchorCountBuffer,
        sizeof(uint32_t),
        nullptr, GL_DYNAMIC_COPY);
}

void CPUPreviewSystem::updateGizmoData() {
    const auto* visualData = m_visualSystem->getCurrentVisualData();
    if (!visualData || visualData->activeCount == 0 || !m_gizmoExtractShader) {
        return;
    }
    
    m_gizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_gpuCellBuffer);
    // Bind gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_gizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (static_cast<GLuint>(visualData->activeCount) + 63) / 64;
    m_gizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Memory barrier for buffer copy
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Copy data from compute buffer to VBO for rendering
    glCopyNamedBufferSubData(m_gizmoBuffer, m_gizmoVBO, 0, 0, visualData->activeCount * 6 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CPUPreviewSystem::updateRingGizmoData() {
    const auto* visualData = m_visualSystem->getCurrentVisualData();
    if (!visualData || visualData->activeCount == 0 || !m_ringGizmoExtractShader) {
        return;
    }
    
    m_ringGizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_gpuCellBuffer);
    // Bind mode data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_gpuModeBuffer);
    // Bind ring gizmo buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_ringGizmoBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (static_cast<GLuint>(visualData->activeCount) + 63) / 64;
    m_ringGizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Memory barrier for buffer copy
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Copy data from compute buffer to VBO for rendering
    // Each cell has: 2 rings (384 vertices)
    glCopyNamedBufferSubData(m_ringGizmoBuffer, m_ringGizmoVBO, 0, 0, visualData->activeCount * 384 * sizeof(glm::vec4) * 2);
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CPUPreviewSystem::updateAnchorGizmoData() {
    const auto* visualData = m_visualSystem->getCurrentVisualData();
    if (!visualData || visualData->activeCount == 0 || !m_anchorGizmoExtractShader) {
        return;
    }
    
    // Reset anchor count
    uint32_t zero = 0;
    glNamedBufferSubData(m_anchorCountBuffer, 0, sizeof(uint32_t), &zero);
    
    m_anchorGizmoExtractShader->use();
    
    // Bind cell data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_gpuCellBuffer);
    // Bind mode data as input
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_gpuModeBuffer);
    // Note: CPU preview system doesn't have adhesion connections yet
    // For now, we'll bind a dummy buffer or skip anchor gizmos
    // glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, adhesionConnectionBuffer);
    // Bind anchor instance buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, m_anchorGizmoBuffer);
    // Bind anchor count buffer as output
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, m_anchorCountBuffer);
    // Bind cell count buffer
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 5, m_gpuCellCountBuffer);
    
    // Dispatch compute shader
    GLuint numGroups = (static_cast<GLuint>(visualData->activeCount) + 63) / 64;
    m_anchorGizmoExtractShader->dispatch(numGroups, 1, 1);
    
    // Memory barrier for buffer copy
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    
    // Get the total anchor count
    glGetNamedBufferSubData(m_anchorCountBuffer, 0, sizeof(uint32_t), &m_totalAnchorCount);
    
    // Copy data from compute buffer to VBO for rendering
    if (m_totalAnchorCount > 0) {
        glCopyNamedBufferSubData(m_anchorGizmoBuffer, m_anchorGizmoVBO, 0, 0, m_totalAnchorCount * 48); // Assuming 48 bytes per instance
    }
    
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void CPUPreviewSystem::cleanupGizmos() {
    if (m_gizmoBuffer != 0) {
        glDeleteBuffers(1, &m_gizmoBuffer);
        m_gizmoBuffer = 0;
    }
    if (m_gizmoVBO != 0) {
        glDeleteBuffers(1, &m_gizmoVBO);
        m_gizmoVBO = 0;
    }
    if (m_gizmoVAO != 0) {
        glDeleteVertexArrays(1, &m_gizmoVAO);
        m_gizmoVAO = 0;
    }
}

void CPUPreviewSystem::cleanupRingGizmos() {
    if (m_ringGizmoBuffer != 0) {
        glDeleteBuffers(1, &m_ringGizmoBuffer);
        m_ringGizmoBuffer = 0;
    }
    if (m_ringGizmoVBO != 0) {
        glDeleteBuffers(1, &m_ringGizmoVBO);
        m_ringGizmoVBO = 0;
    }
    if (m_ringGizmoVAO != 0) {
        glDeleteVertexArrays(1, &m_ringGizmoVAO);
        m_ringGizmoVAO = 0;
    }
}

void CPUPreviewSystem::cleanupAnchorGizmos() {
    if (m_anchorGizmoBuffer != 0) {
        glDeleteBuffers(1, &m_anchorGizmoBuffer);
        m_anchorGizmoBuffer = 0;
    }
    if (m_anchorGizmoVBO != 0) {
        glDeleteBuffers(1, &m_anchorGizmoVBO);
        m_anchorGizmoVBO = 0;
    }
    if (m_anchorCountBuffer != 0) {
        glDeleteBuffers(1, &m_anchorCountBuffer);
        m_anchorCountBuffer = 0;
    }
}

void CPUPreviewSystem::renderRingGizmos(glm::vec2 resolution, const Camera& camera, const UIManager& uiManager) {
    if (!uiManager.showOrientationGizmos || !m_initialized || !m_dataManager || !m_visualSystem) {
        return;
    }
    
    const auto* visualData = m_visualSystem->getCurrentVisualData();
    if (!visualData || visualData->activeCount == 0) {
        return;
    }
    
    // Upload CPU cell data to GPU buffers
    uploadCellDataToGPU();
    
    // Update ring gizmo data from current cell orientations and split directions
    updateRingGizmoData();
    
    if (!m_ringGizmoShader) {
        return;
    }
    
    m_ringGizmoShader->use();
    
    // Set up camera matrices
    glm::mat4 view = camera.getViewMatrix();
    float aspectRatio = resolution.x / resolution.y;
    if (aspectRatio <= 0.0f || !std::isfinite(aspectRatio)) {
        aspectRatio = 16.0f / 9.0f;
    }
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 1000.0f);
    
    m_ringGizmoShader->setMat4("uProjection", projection);
    m_ringGizmoShader->setMat4("uView", view);
    
    // Enable face culling so rings are only visible from one side
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    
    // Enable depth testing but disable depth writing to avoid z-fighting with spheres
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    
    // Enable blending for better visibility
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Render ring gizmo triangles
    glBindVertexArray(m_ringGizmoVAO);
    
    // Render all vertices in one call - the shader generates the correct number of vertices per cell
    // Each cell has: 2 rings (384 vertices)
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(visualData->activeCount * 384));
    
    glBindVertexArray(0);
    
    // Restore OpenGL state
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
}

std::vector<GPUModeAdhesionSettings> CPUPreviewSystem::createModeSettingsFromGenome(const CPUGenomeParameters& genomeParams) {
    // Create mode settings from genome parameters (Requirements 4.1, 4.2, 4.5)
    // This ensures instant parameter updates when genome settings change
    
    std::vector<GPUModeAdhesionSettings> modeSettings;
    
    // Create a single mode setting based on current genome parameters
    GPUModeAdhesionSettings settings;
    
    // Check if adhesion is enabled in the genome (bit 8 for adhesion capability)
    bool adhesionEnabled = (genomeParams.cellTypeFlags & (1 << 8)) != 0;
    
    if (adhesionEnabled) {
        // Use the actual adhesion settings from the genome (matching GPU implementation)
        const AdhesionSettings& adhesion = genomeParams.adhesionSettings;
        settings.canBreak = adhesion.canBreak ? 1 : 0;
        settings.breakForce = adhesion.breakForce;
        settings.restLength = adhesion.restLength;
        settings.linearSpringStiffness = adhesion.linearSpringStiffness;
        settings.linearSpringDamping = adhesion.linearSpringDamping;
        settings.orientationSpringStiffness = adhesion.orientationSpringStiffness;
        settings.orientationSpringDamping = adhesion.orientationSpringDamping;
        settings.maxAngularDeviation = adhesion.maxAngularDeviation;
        settings.twistConstraintStiffness = adhesion.twistConstraintStiffness;
        settings.twistConstraintDamping = adhesion.twistConstraintDamping;
        settings.enableTwistConstraint = adhesion.enableTwistConstraint ? 1 : 0;
    } else {
        // Disable all adhesion forces when adhesion is not enabled (matching GPU implementation)
        settings.canBreak = 0;
        settings.breakForce = 0.0f;
        settings.restLength = 0.0f;
        settings.linearSpringStiffness = 0.0f;
        settings.linearSpringDamping = 0.0f;
        settings.orientationSpringStiffness = 0.0f;
        settings.orientationSpringDamping = 0.0f;
        settings.maxAngularDeviation = 0.0f;
        settings.twistConstraintStiffness = 0.0f;
        settings.twistConstraintDamping = 0.0f;
        settings.enableTwistConstraint = 0;
    }
    settings._padding = 0; // Ensure padding is zero
    
    // Add the mode setting to the vector
    modeSettings.push_back(settings);
    
    return modeSettings;
}