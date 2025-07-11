#pragma once
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "../../simulation/cell/cell_manager.h"
#include "../../simulation/cell/common_structs.h"
#include "../../scene/scene_manager.h"

class GenomeEditorPanel
{
public:
    void render(CellManager& cellManager, SceneManager& sceneManager);
    GenomeData currentGenome;
    bool genomeChanged = false;          // Flag to indicate genome was modified

private:
    bool windowsLocked = true;
    int getWindowFlags(int baseFlags = 0) const;
    
    // Genome Editor Helper Functions
    void drawModeSelector(GenomeData &genome);
    void drawModeSettings(ModeSettings &mode, int modeIndex, CellManager& cellManager);
    void drawParentSettings(ModeSettings &mode);
    void drawChildSettings(const char *label, ChildSettings &child);
    void drawAdhesionSettings(AdhesionSettings &adhesion);
    void drawSliderWithInput(const char *label, float *value, float min, float max, const char *format = "%.2f", float step = 0.0f);
    void drawColorPicker(const char *label, glm::vec3 *color);
    glm::vec3 normalizeColor(const glm::vec3& color); // Helper to normalize color values
    void validateGenomeColors(); // Validate and fix color values in genome
    void addTooltip(const char* tooltip); // Helper to add question mark tooltips
    bool isColorBright(const glm::vec3 &color); // Helper to determine if color is bright

    // Genome Editor Data
    int selectedModeIndex = 0;
}; 