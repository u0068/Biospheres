#pragma once

// Third-party includes
#include <glad/glad.h>
#include "../../third_party/imgui/imgui.h"
#include <glm/glm.hpp>

// Standard includes
#include <string>
#include <vector>
#include <functional>

// Local includes
#include "mesh_baker.h"

// Forward declarations
class MeshBaker;

/**
 * Configuration structure for baking parameters
 */
struct BakingConfig {
    // Output settings
    std::string outputDirectory = "assets/meshes/bridges/";
    
    // Quality settings
    int resolution = 64;              // 32, 64, or 128
    
    // Size ratio configuration (larger radius / smaller radius)
    // Optimized for 1:4 maximum ratio with high quality interpolation
    std::vector<float> sizeRatios = {1.0f, 1.3f, 1.6f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f};
    
    // Distance ratio configuration (distance / combined radius)  
    // Optimized for bridge shape transitions: dense in critical 0.1-2.0 range
    std::vector<float> distanceRatios = {0.1f, 0.4f, 0.7f, 1.0f, 1.3f, 1.6f, 2.0f, 2.5f, 3.0f};
    
    // SDF blending parameters
    float blendingStrength = 0.5f;     // Base blending strength (0.1 - 5.0)
    
    // Blend strength curve control points (distance ratio -> blend multiplier)
    // Using BlendCurvePoint from mesh_baker.h
    std::vector<BlendCurvePoint> blendCurvePoints = {
        {0.1f, 0.5f},   // Close: reduced blending
        {1.0f, 1.0f},   // Medium: normal blending
        {2.0f, 1.5f},   // Far: increased blending
        {3.0f, 0.2f}    // Very far: minimal blending
    };
    
    // Validation methods
    bool validateSizeRatio(float ratio) const {
        return ratio >= 1.0f && ratio <= 4.0f;
    }
    
    bool validateDistanceRatio(float ratio) const {
        return ratio >= 0.1f && ratio <= 3.0f;
    }
    
    bool validateBlendingStrength(float strength) const {
        return strength >= 0.1f && strength <= 10.0f;
    }
    
    bool validateDistanceScaling(float scaling) const {
        return scaling >= 0.0f && scaling <= 1.0f;
    }
    
    bool validateScalingRatio(float ratio) const {
        return ratio >= 0.0f && ratio <= 3.0f;
    }
    

    
    // Calculate total mesh count
    int getTotalMeshCount() const {
        return static_cast<int>(sizeRatios.size() * distanceRatios.size());
    }
    
    // Get resolution options
    static std::vector<int> getResolutionOptions() {
        return {32, 64, 128};
    }
};

/**
 * ImGui-based user interface for the Baker Tool
 */
class BakerUI {
public:
    BakerUI();
    ~BakerUI() = default;
    
    // Main render method
    void render(MeshBaker& meshBaker);
    
    // Configuration access
    const BakingConfig& getConfig() const { return config; }
    
    // Current preview parameters
    float getCurrentSizeRatio() const { return currentSizeRatio; }
    float getCurrentDistanceRatio() const { return currentDistanceRatio; }
    
    // Viewport info
    glm::vec2 getViewportPos() const { return viewportPos; }
    glm::vec2 getViewportSize() const { return viewportSize; }
    void setPreviewTexture(GLuint texture) { previewTexture = texture; }
    
    // Mesh preview
    const BakedMesh* getTestMesh() const { return testMesh.isValid() ? &testMesh : nullptr; }
    bool shouldShowMeshPreview() const { return showMeshPreview; }
    bool shouldShowWireframe() const { return showWireframe; }
    bool hasMeshChanged() { bool changed = meshChanged; meshChanged = false; return changed; }
    
private:
    // Configuration
    BakingConfig config;
    
    // Current preview parameters
    float currentSizeRatio = 1.0f;
    float currentDistanceRatio = 1.0f;
    
    // Viewport info
    glm::vec2 viewportPos = glm::vec2(0.0f);
    glm::vec2 viewportSize = glm::vec2(400.0f, 300.0f);
    GLuint previewTexture = 0;
    
    // Mesh preview
    BakedMesh testMesh;
    bool showMeshPreview = false;
    bool showWireframe = false;
    bool meshChanged = false;
    
    // UI state
    bool showProgressDialog = false;
    float bakingProgress = 0.0f;
    std::string bakingStatus = "";
    bool bakingInProgress = false;
    bool bakingCancelled = false;
    int currentMeshIndex = 0;
    int totalMeshes = 0;
    float estimatedTimeRemaining = 0.0f;
    std::string lastError = "";
    bool showCompletionDialog = false;
    bool bakingSucceeded = false;
    
    // Error dialog state
    bool showErrorDialog = false;
    std::string errorTitle = "";
    std::string errorMessage = "";
    std::string errorRecoverySuggestion = "";
    
    // Temporary input buffers
    float newSizeRatio = 1.0f;
    float newDistanceRatio = 1.0f;
    char outputDirBuffer[512];
    
    // UI rendering methods
    void renderMainMenuBar();
    void renderConfigPanel();
    void renderParameterControls();
    void renderBakingControls(MeshBaker& meshBaker);
    void renderProgressDialog();
    void renderCompletionDialog();
    void renderPreviewControls();
    void renderPreviewViewport();
    void renderErrorDialog();
    
    // Configuration management
    void renderSizeRatioConfig();
    void renderDistanceRatioConfig();
    void renderQualitySettings();
    void renderOutputSettings();
    
    // Configuration persistence
    bool saveConfiguration(const std::string& filepath);
    bool loadConfiguration(const std::string& filepath);
    std::string getDefaultConfigPath() const;
    
    // Utility methods
    void addSizeRatio();
    void removeSizeRatio(size_t index);
    void addDistanceRatio();
    void removeDistanceRatio(size_t index);
    
    // Baking operations
    void startBaking(MeshBaker& meshBaker);
    void updateBakingProgress(float progress, const std::string& status);
    void finishBaking();
    void cancelBaking();
    bool isBakingCancelled() const { return bakingCancelled; }
    
    // Validation
    bool validateConfiguration() const;
    std::string getValidationErrors() const;
    
    // Error handling
    void showError(const std::string& title, const std::string& message, const std::string& recoverySuggestion = "");
};