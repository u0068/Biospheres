#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../core/config.h"
#include "../scene/scene_manager.h"
#include "imgui.h"
#include <algorithm>
#include <iostream>

// Handle debounced genome resimulation with immediate update on mouse release
void UIManager::updateDebouncedGenomeResimulation(CellManager& cellManager, SceneManager& sceneManager, float deltaTime)
{
    // Detect mouse release
    bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouseJustReleased = wasMouseDownLastFrame && !isMouseDown;
    wasMouseDownLastFrame = isMouseDown;
    
    // If we have a pending resimulation, update timers
    if (pendingGenomeResimulation)
    {
        genomeChangeDebounceTimer += deltaTime;
        periodicUpdateTimer += deltaTime;
        
        // Periodic genome buffer updates during slider dragging (for instant color/visual updates)
        if (periodicUpdateTimer >= GENOME_PERIODIC_UPDATE_INTERVAL && !isResimulating)
        {
            // Update genome buffer only (fast operation, updates colors immediately)
            cellManager.addGenomeToBuffer(currentGenome);
            periodicUpdateTimer = 0.0f;
        }
        
        // Trigger resimulation on mouse release OR after debounce delay
        bool shouldResimulate = (mouseJustReleased || genomeChangeDebounceTimer >= GENOME_CHANGE_DEBOUNCE_DELAY) && !isResimulating;
        
        if (shouldResimulate)
        {
            isResimulating = true;
            resimulationProgress = 0.0f;
            
            // Reset the simulation with the new genome
            cellManager.resetSimulation();
            cellManager.addGenomeToBuffer(currentGenome);
            ComputeCell newCell{};
            newCell.modeIndex = currentGenome.initialMode;
            // Set initial cell orientation to the genome's initial orientation
            // This keeps the initial cell orientation independent of Child A/B settings
            newCell.orientation = currentGenome.initialOrientation;
            cellManager.addCellToStagingBuffer(newCell);
            cellManager.addStagedCellsToQueueBuffer(); // Force immediate GPU buffer sync
            
            // Reset simulation time
            sceneManager.resetPreviewSimulationTime();
            
            // If time scrubber is at a specific time, fast-forward to that time
            if (currentTime > 0.0f)
            {
                // Temporarily pause to prevent normal time updates during fast-forward
                bool wasPaused = sceneManager.isPaused();
                sceneManager.setPaused(true);
                
                // Use optimized frame-skipping resimulation with progress tracking
                int framesSkipped = cellManager.updateCellsFastForwardOptimized(
                    currentTime, 
                    config::resimulationTimeStep
                );
                
                // Update simulation time to final time
                sceneManager.setPreviewSimulationTime(currentTime);
                
                // Log performance improvement if any frames were skipped
                if (framesSkipped > 0) {
                    std::cout << "Frame skipping saved " << framesSkipped 
                              << " frames during resimulation\n";
                }
                
                // Restore original pause state after fast-forward
                sceneManager.setPaused(wasPaused);
            }
            else
            {
                // If at time 0, just advance simulation by one frame after reset
                cellManager.updateCellsFastForward(config::resimulationTimeStep);
                sceneManager.setPreviewSimulationTime(0.0f);
            }
            
            // Clear the pending flag and reset timers
            pendingGenomeResimulation = false;
            genomeChangeDebounceTimer = 0.0f;
            periodicUpdateTimer = 0.0f;
            isResimulating = false;
            resimulationProgress = 1.0f;
        }
    }
}
