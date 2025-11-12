#include "scene_manager.h"
#include "../core/config.h"
#include "../simulation/cpu_preview/cpu_preview_system.h"
#include "../simulation/cell/cell_manager.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>

// Scene file management implementation (Requirements 3.4, 3.5)

void SceneManager::loadPreviewScene(const std::string& filename) {
    // Load .soa file for CPU Preview System
    std::string soaFilename = filename;
    if (!soaFilename.ends_with(".soa")) {
        soaFilename += ".soa";
    }
    
    if (!std::filesystem::exists(soaFilename)) {
        std::cerr << "Preview scene file not found: " << soaFilename << std::endl;
        return;
    }
    
    currentPreviewSceneFile = soaFilename;
    std::cout << "Loading Preview scene from: " << soaFilename << std::endl;
    
    // Coordinate with CPU Preview System for actual loading
    if (m_cpuPreviewSystem) {
        try {
            m_cpuPreviewSystem->loadPreviewScene(soaFilename);
            std::cout << "Preview scene loaded successfully via CPU Preview System\n";
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to load preview scene: " << e.what() << std::endl;
        }
    }
}

void SceneManager::savePreviewScene(const std::string& filename) {
    // Save .soa file for CPU Preview System
    std::string soaFilename = filename;
    if (!soaFilename.ends_with(".soa")) {
        soaFilename += ".soa";
    }
    
    currentPreviewSceneFile = soaFilename;
    std::cout << "Saving Preview scene to: " << soaFilename << std::endl;
    
    // Coordinate with CPU Preview System for actual saving
    if (m_cpuPreviewSystem) {
        try {
            m_cpuPreviewSystem->savePreviewScene(soaFilename);
            std::cout << "Preview scene saved successfully via CPU Preview System\n";
        }
        catch (const std::exception& e) {
            std::cerr << "Failed to save preview scene: " << e.what() << std::endl;
        }
    }
}

void SceneManager::loadMainScene(const std::string& filename) {
    // Load .aos file for GPU Main Simulation System
    std::string aosFilename = filename;
    if (!aosFilename.ends_with(".aos")) {
        aosFilename += ".aos";
    }
    
    if (!std::filesystem::exists(aosFilename)) {
        std::cerr << "Main scene file not found: " << aosFilename << std::endl;
        return;
    }
    
    currentMainSceneFile = aosFilename;
    std::cout << "Loading Main scene from: " << aosFilename << std::endl;
    
    // Note: Actual loading will be handled by CellManager
    // This method provides the interface for scene management coordination
}

void SceneManager::saveMainScene(const std::string& filename) {
    // Save .aos file for GPU Main Simulation System
    std::string aosFilename = filename;
    if (!aosFilename.ends_with(".aos")) {
        aosFilename += ".aos";
    }
    
    currentMainSceneFile = aosFilename;
    std::cout << "Saving Main scene to: " << aosFilename << std::endl;
    
    // Note: Actual saving will be handled by CellManager
    // This method provides the interface for scene management coordination
}

void SceneManager::switchToPreviewMode() {
    if (currentScene != Scene::PreviewSimulation) {
        std::cout << "Switching to Preview Simulation mode (CPU native SoA)" << std::endl;
        
        // Store current main system state
        if (currentScene == Scene::MainSimulation) {
            mainPaused = paused;
            mainSystemActive = false;
        }
        
        // Switch to preview scene - NO DATA CONVERSION (Requirement 3.4)
        switchToScene(Scene::PreviewSimulation);
        
        // Activate preview system
        previewSystemActive = true;
        paused = previewPaused;
        
        // Ensure CPU Preview System is enabled
        if (m_cpuPreviewSystem) {
            m_cpuPreviewSystem->setEnabled(true);
        }
        
        std::cout << "Preview mode activated - using CPU native SoA data format (no conversion)" << std::endl;
    } else {
        // Already in preview mode, but ensure system is active
        previewSystemActive = true;
        
        // Ensure CPU Preview System is enabled
        if (m_cpuPreviewSystem) {
            m_cpuPreviewSystem->setEnabled(true);
        }
        
        std::cout << "Preview mode already active - ensuring CPU Preview System is enabled" << std::endl;
    }
}

void SceneManager::switchToMainMode() {
    if (currentScene != Scene::MainSimulation) {
        std::cout << "Switching to Main Simulation mode (GPU native AoS)" << std::endl;
        
        // Store current preview system state
        if (currentScene == Scene::PreviewSimulation) {
            previewPaused = paused;
            previewSystemActive = false;
            
            // Disable CPU Preview System when switching away
            if (m_cpuPreviewSystem) {
                m_cpuPreviewSystem->setEnabled(false);
            }
        }
        
        // Switch to main scene - NO DATA CONVERSION (Requirement 3.4)
        switchToScene(Scene::MainSimulation);
        
        // Activate main system
        mainSystemActive = true;
        paused = mainPaused;
        
        std::cout << "Main mode activated - using GPU native AoS data format (no conversion)" << std::endl;
    }
}

// Scene-specific capacity management implementation
void SceneManager::setSceneMaxCapacity(Scene scene, int capacity) {
    // Validate against system limits
    if (scene == Scene::PreviewSimulation) {
        // Validate CPU Preview capacity
        if (capacity <= 0 || capacity > config::CPU_PREVIEW_MAX_CAPACITY) {
            std::cerr << "ERROR: CPU Preview capacity " << capacity 
                      << " must be between 1 and " << config::CPU_PREVIEW_MAX_CAPACITY << std::endl;
            // Use assertion for debug builds
            assert(capacity > 0 && capacity <= config::CPU_PREVIEW_MAX_CAPACITY && 
                   "CPU Preview capacity exceeds maximum");
            return;
        }
    } else if (scene == Scene::MainSimulation) {
        // Validate GPU Main capacity
        if (capacity <= 0 || capacity > config::GPU_MAIN_MAX_CAPACITY) {
            std::cerr << "ERROR: GPU Main capacity " << capacity 
                      << " must be between 1 and " << config::GPU_MAIN_MAX_CAPACITY << std::endl;
            // Use assertion for debug builds
            assert(capacity > 0 && capacity <= config::GPU_MAIN_MAX_CAPACITY && 
                   "GPU Main capacity exceeds maximum");
            return;
        }
    }
    
    sceneMaxCapacities[scene] = capacity;
}

int SceneManager::getSceneMaxCapacity(Scene scene) const {
    auto it = sceneMaxCapacities.find(scene);
    if (it != sceneMaxCapacities.end()) {
        return it->second;
    }
    
    // Return default capacity based on scene type
    if (scene == Scene::PreviewSimulation) {
        return config::CPU_PREVIEW_MAX_CAPACITY;
    } else if (scene == Scene::MainSimulation) {
        return config::GPU_MAIN_MAX_CAPACITY;
    }
    
    return config::CPU_PREVIEW_MAX_CAPACITY; // Fallback
}

int SceneManager::getCurrentSceneMaxCapacity() const {
    return getSceneMaxCapacity(currentScene);
}