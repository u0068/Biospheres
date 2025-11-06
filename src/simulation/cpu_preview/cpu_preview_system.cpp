#include "cpu_preview_system.h"
#include "../../core/config.h"
#include <chrono>
#include <stdexcept>
#include <iostream>

CPUPreviewSystem::CPUPreviewSystem() 
    : m_dataManager(std::make_unique<CPUSoADataManager>())
    , m_physicsEngine(std::make_unique<CPUSIMDPhysicsEngine>())
    , m_visualSystem(std::make_unique<CPUTripleBufferSystem>())
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
        
        // Create default empty scene
        createEmptyScene(256);
        
        m_initialized = true;
        m_enabled = true;
        
        std::cout << "CPU Preview System initialized successfully\n";
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
    
    m_initialized = false;
    std::cout << "CPU Preview System shutdown complete\n";
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
        // Validate system state
        validateSystemState();

        // Run physics simulation
        if (m_physicsEngine && m_dataManager) {
            m_physicsEngine->simulateStep(
                m_dataManager->getCellData(),
                m_dataManager->getAdhesionData(),
                deltaTime
            );
        }

        // Update visual data asynchronously
        if (m_visualSystem && m_dataManager) {
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

void CPUPreviewSystem::reset() {
    if (!m_initialized) {
        return;
    }
    
    // Reset to empty scene
    createEmptyScene(256);
    m_lastSimulationTime = 0.0f;
}

void CPUPreviewSystem::updateGenomeParameters(uint32_t cellIndex, const CPUGenomeParameters& params) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    m_dataManager->updateGenomeParameters(cellIndex, params);
}

void CPUPreviewSystem::applyGenomeToAll(uint32_t genomeID, const CPUGenomeParameters& params) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    // Apply genome parameters to all cells with matching genomeID
    const auto& cellData = m_dataManager->getCellData();
    for (size_t i = 0; i < cellData.activeCellCount; ++i) {
        if (cellData.genomeID[i] == genomeID) {
            m_dataManager->updateGenomeParameters(static_cast<uint32_t>(i), params);
        }
    }
}

uint32_t CPUPreviewSystem::addCell(const CPUCellParameters& params) {
    if (!m_dataManager) {
        throw std::runtime_error("Data manager not initialized");
    }
    
    return m_dataManager->addCell(params);
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
    
    // Validate data integrity
    m_dataManager->validateDataIntegrity();
}

void CPUPreviewSystem::updatePerformanceMetrics() {
    auto frameEnd = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - m_frameStart);
    m_lastSimulationTime = duration.count() / 1000.0f; // Convert to milliseconds
}