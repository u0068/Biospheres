#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../core/config.h"
#include "imgui.h"
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>

void UIManager::initializeKeyframes(CellManager& cellManager)
{
    // Save current time slider position
    float savedCurrentTime = currentTime;
    float savedTargetTime = targetTime;
    
    // Clear existing keyframes
    keyframes.clear();
    keyframes.resize(MAX_KEYFRAMES);
    
    // Reset simulation to initial state
    cellManager.resetSimulation();
    cellManager.addGenomeToBuffer(currentGenome);
    ComputeCell newCell{};
    newCell.modeIndex = currentGenome.initialMode;
    cellManager.addCellToStagingBuffer(newCell);
    cellManager.addStagedCellsToQueueBuffer();
    
    // Capture initial keyframe at time 0
    captureKeyframe(cellManager, 0.0f, 0);
    
    // Calculate time interval between keyframes
    float timeInterval = maxTime / (MAX_KEYFRAMES - 1);
    
    // Simulate and capture keyframes (keeping your original time step logic)
    for (int i = 1; i < MAX_KEYFRAMES; i++)
    {
        float targetTime = i * timeInterval;
        float currentSimTime = (i - 1) * timeInterval;
        
        // Simulate from previous keyframe to current keyframe
        float timeToSimulate = targetTime - currentSimTime;
        float scrubTimeStep = config::physicsTimeStep;
        
        while (timeToSimulate > 0.0f)
        {
            float stepTime = (timeToSimulate > scrubTimeStep) ? scrubTimeStep : timeToSimulate;
            cellManager.updateCellsFastForward(stepTime); // Use optimized fast-forward
            timeToSimulate -= stepTime;
        }
        
        // Capture keyframe
        captureKeyframe(cellManager, targetTime, i);
    }
    
    keyframesInitialized = true;
    // Check for potential timing accuracy issues with keyframe intervals
    checkKeyframeTimingAccuracy();

    // Restore time slider position and trigger simulation reset to that time
    currentTime = std::max(0.0f, std::min(savedCurrentTime, maxTime));
    targetTime = currentTime;
    needsSimulationReset = true;
    isScrubbingTime = true;
}

void UIManager::updateKeyframes(CellManager& cellManager, float newMaxTime)
{
    // Save current time slider position
    float savedCurrentTime = currentTime;
    float savedTargetTime = targetTime;
    maxTime = newMaxTime;
    keyframesInitialized = false;
    initializeKeyframes(cellManager);
    // Restore time slider position and trigger simulation reset to that time
    currentTime = std::max(0.0f, std::min(savedCurrentTime, maxTime));
    targetTime = currentTime;
    needsSimulationReset = true;
    isScrubbingTime = true;
}

int UIManager::findNearestKeyframe(float targetTime) const
{
    if (!keyframesInitialized || keyframes.empty())
        return 0;
    
    // Clamp target time to valid range
    targetTime = std::max(0.0f, std::min(targetTime, maxTime));
    
    // Calculate which keyframe index should contain this time
    float timeInterval = maxTime / (MAX_KEYFRAMES - 1);
    int idealIndex = static_cast<int>(targetTime / timeInterval);
    
    // Clamp to valid keyframe range
    idealIndex = std::max(0, std::min(idealIndex, MAX_KEYFRAMES - 1));
    
    // Find the nearest valid keyframe at or before the ideal index
    for (int i = idealIndex; i >= 0; i--)
    {
        if (i < keyframes.size() && keyframes[i].isValid)
        {
            return i;
        }
    }
    
    // Fallback to keyframe 0 if nothing found
    return 0;
}

void UIManager::restoreFromKeyframe(CellManager& cellManager, int keyframeIndex)
{
    if (keyframeIndex < 0 || keyframeIndex >= keyframes.size() || !keyframes[keyframeIndex].isValid)
        return;
    

    
    // Reset simulation (this clears adhesion connections)
    cellManager.resetSimulation();
    
    // Restore genome (make a non-const copy)
    GenomeData genomeCopy = keyframes[keyframeIndex].genome;
    cellManager.addGenomeToBuffer(genomeCopy);
    
    // CRITICAL FIX: Use direct restoration method that bypasses addition buffer system
    if (keyframes[keyframeIndex].cellCount > 0) {
        // Restore cells directly to GPU buffer
        cellManager.restoreCellsDirectlyToGPUBuffer(keyframes[keyframeIndex].cellStates);
        
        // Update CPU cell data to match GPU
        cellManager.setCPUCellData(keyframes[keyframeIndex].cellStates);
        
        // Use targeted barrier for better performance
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
        
        // Force update of spatial grid after restoration
        cellManager.updateSpatialGrid();
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }
    
    // Restore adhesion connections AFTER cell restoration
    if (keyframes[keyframeIndex].adhesionCount > 0) {
        cellManager.restoreAdhesionConnections(keyframes[keyframeIndex].adhesionConnections, keyframes[keyframeIndex].adhesionCount);
    }
    
    // Update CPU-side counts to match GPU state
    cellManager.updateCounts();
    
    // Verify restoration by checking first cell position and age
    if (keyframes[keyframeIndex].cellCount > 0) {
        cellManager.syncCellPositionsFromGPU();
        ComputeCell verifyCell = cellManager.getCellData(0);
        const ComputeCell& expectedCell = keyframes[keyframeIndex].cellStates[0];
        
        float posDiff = glm::length(glm::vec3(verifyCell.positionAndMass) - glm::vec3(expectedCell.positionAndMass));
        float ageDiff = abs(verifyCell.age - expectedCell.age);
        
        if (posDiff > 0.001f) {
            std::cout << "WARNING: Keyframe restoration position accuracy issue! Position difference: " 
                      << posDiff << "\n";
            std::cout << "Expected: (" << expectedCell.positionAndMass.x << ", " 
                      << expectedCell.positionAndMass.y << ", " << expectedCell.positionAndMass.z << ")\n";
            std::cout << "Actual: (" << verifyCell.positionAndMass.x << ", " 
                      << verifyCell.positionAndMass.y << ", " << verifyCell.positionAndMass.z << ")\n";
        }
        
        if (ageDiff > 0.001f) {
            std::cout << "WARNING: Keyframe restoration age accuracy issue! Age difference: " 
                      << ageDiff << " (Expected: " << expectedCell.age << ", Actual: " << verifyCell.age << ")\n";
        }
    }
}

void UIManager::captureKeyframe(CellManager& cellManager, float time, int keyframeIndex)
{
    if (keyframeIndex < 0 || keyframeIndex >= MAX_KEYFRAMES)
    {
        std::cerr << "Invalid keyframe index for capture: " << keyframeIndex << "\n";
        return;
    }
    
    // Ensure keyframes vector is large enough
    if (keyframeIndex >= keyframes.size())
    {
        keyframes.resize(keyframeIndex + 1);
    }
    
    SimulationKeyframe& keyframe = keyframes[keyframeIndex];
    
    // Use targeted barrier for better performance - only sync compute shader storage
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
    
    // Capture current simulation state
    keyframe.time = time;
    keyframe.genome = currentGenome;
    keyframe.cellCount = cellManager.getCellCount();
    
    // Sync cell data from GPU to CPU to ensure we have latest state
    cellManager.syncCellPositionsFromGPU();
    
    // Copy cell states
    keyframe.cellStates.clear();
    keyframe.cellStates.reserve(keyframe.cellCount);
    
    for (int i = 0; i < keyframe.cellCount; i++)
    {
        keyframe.cellStates.push_back(cellManager.getCellData(i));
    }
    
    // Capture adhesion connections
    keyframe.adhesionConnections = cellManager.getAdhesionConnections();
    keyframe.adhesionCount = static_cast<int>(keyframe.adhesionConnections.size());
    
    keyframe.isValid = true;
}

void UIManager::checkKeyframeTimingAccuracy()
{
    if (!keyframesInitialized || currentGenome.modes.empty()) {
        return;
    }
    
    // Find the shortest split interval in the genome
    float shortestSplitInterval = FLT_MAX;
    for (const auto& mode : currentGenome.modes) {
        if (mode.splitInterval < shortestSplitInterval) {
            shortestSplitInterval = mode.splitInterval;
        }
    }
    
    // Calculate keyframe interval
    float keyframeInterval = maxTime / (MAX_KEYFRAMES - 1);
    
    // Check if keyframe intervals are too large compared to split timing
    float timingRatio = keyframeInterval / shortestSplitInterval;
    

}