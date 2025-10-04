#include "ui_manager.h"
#include "../simulation/cell/cell_manager.h"
#include "../core/config.h"
#include "../scene/scene_manager.h"
#include <algorithm>
#include <iostream>

// Handle debounced genome resimulation to prevent overshooting during rapid adjustments
void UIManager::updateDebouncedGenomeResimulation(CellManager& cellManager, SceneManager& sceneManager, float deltaTime)
{
    // If we have a pending resimulation, update the debounce timer
    if (pendingGenomeResimulation)
    {
        genomeChangeDebounceTimer += deltaTime;
        
        // Once the debounce delay has passed, perform the resimulation
        if (genomeChangeDebounceTimer >= GENOME_CHANGE_DEBOUNCE_DELAY)
        {
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
                
                // Use the same time step as live simulation for consistent physics
                float scrubTimeStep = config::physicsTimeStep;
                float timeRemaining = currentTime;
                int maxSteps = (int)(currentTime / scrubTimeStep) + 1;
                
                for (int i = 0; i < maxSteps && timeRemaining > 0.0f; ++i)
                {
                    float stepTime = (timeRemaining > scrubTimeStep) ? scrubTimeStep : timeRemaining;
                    cellManager.updateCellsFastForward(stepTime); // Use optimized fast-forward
                    timeRemaining -= stepTime;
                    
                    // Update simulation time manually during fast-forward
                    sceneManager.setPreviewSimulationTime(currentTime - timeRemaining);
                }
                
                // Restore original pause state after fast-forward
                sceneManager.setPaused(wasPaused);
            }
            else
            {
                // If at time 0, just advance simulation by one frame after reset
                cellManager.updateCellsFastForward(config::physicsTimeStep); // Use optimized fast-forward
            }
            
            // Clear the pending flag and reset timer
            pendingGenomeResimulation = false;
            genomeChangeDebounceTimer = 0.0f;
        }
    }
}
