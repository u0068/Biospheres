#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../rendering/camera/camera.h"
#include "../simulation/cell/common_structs.h"

// Panel components
#include "panels/performance_panel.h"
#include "panels/cell_inspector_panel.h"
#include "panels/camera_controls_panel.h"
#include "panels/genome_editor_panel.h"
#include "panels/time_scrubber_panel.h"
#include "panels/scene_switcher_panel.h"
#include "panels/tool_panel.h"

// Forward declarations
struct CellManager;
class SceneManager;

class UIManager
{
public:
    // Main render functions (delegated to panels)
    void renderCellInspector(CellManager &cellManager, SceneManager& sceneManager) {
        cellInspectorPanel.render(cellManager, sceneManager);
    }
    
    void renderPerformanceMonitor(CellManager &cellManager, PerformanceMonitor &perfMonitor, SceneManager& sceneManager) {
        performancePanel.render(cellManager, perfMonitor, sceneManager);
    }
    
    void renderCameraControls(CellManager &cellManager, Camera &camera, SceneManager& sceneManager) {
        cameraControlsPanel.render(cellManager, camera, sceneManager);
    }
    
    void renderGenomeEditor(CellManager& cellManager, SceneManager& sceneManager) {
        genomeEditorPanel.render(cellManager, sceneManager);
    }
    
    void renderTimeScrubber(CellManager& cellManager, SceneManager& sceneManager) {
        timeScrubberPanel.render(cellManager, sceneManager);
    }
    
    void renderSceneSwitcher(SceneManager& sceneManager, CellManager& previewCellManager, CellManager& mainCellManager) {
        sceneSwitcherPanel.render(sceneManager, previewCellManager, mainCellManager);
    }

    // Performance monitoring helpers
    void updatePerformanceMetrics(PerformanceMonitor &perfMonitor, float deltaTime) {
        performancePanel.updatePerformanceMetrics(perfMonitor, deltaTime);
    }

    // Preview simulation time control
    void updatePreviewSimulation(CellManager& previewCellManager) {
        timeScrubberPanel.updatePreviewSimulation(previewCellManager);
    }

    // Scene management
    void switchToScene(int sceneIndex) {
        sceneSwitcherPanel.switchToScene(sceneIndex);
    }

    void checkKeyframeTimingAccuracy() {
        timeScrubberPanel.checkKeyframeTimingAccuracy();
    }

    // Access to panel data
    GenomeData& getCurrentGenome() { return genomeEditorPanel.currentGenome; }
    bool isGenomeChanged() const { return genomeEditorPanel.genomeChanged; }
    void setGenomeChanged(bool changed) { genomeEditorPanel.genomeChanged = changed; }

    // Visualization toggles
    bool showOrientationGizmos = false;  // Toggle for showing cell orientation gizmos
    bool showAdhesionLines = true;      // Toggle for showing adhesion lines between sibling cells
    bool wireframeMode = false;          // Toggle for wireframe rendering mode
    bool enableFrustumCulling = true;    // Toggle for frustum culling
    bool enableDistanceCulling = true;   // Toggle for distance-based culling and fading

private:
    // Panel components
    PerformancePanel performancePanel;
    CellInspectorPanel cellInspectorPanel;
    CameraControlsPanel cameraControlsPanel;
    GenomeEditorPanel genomeEditorPanel;
    TimeScrubberPanel timeScrubberPanel;
    SceneSwitcherPanel sceneSwitcherPanel;
    ToolPanel toolPanel;
}; 