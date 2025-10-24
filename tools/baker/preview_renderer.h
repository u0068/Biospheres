#pragma once

// Third-party includes
#include <glad/glad.h>
#include <glm/glm.hpp>

// Standard includes
#include <memory>

// Forward declarations
struct BakingConfig;
struct BakedMesh;
class CameraController;
class Shader;

/**
 * Real-time raymarching preview renderer
 * Displays metaball geometry using raymarching for immediate feedback
 */
class PreviewRenderer {
public:
    PreviewRenderer();
    ~PreviewRenderer();
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Rendering
    void render(const BakingConfig& config, float sizeRatio, float distanceRatio, 
                const CameraController& camera, const glm::vec2& viewportSize);
    
    // Mesh preview
    void setPreviewMesh(const BakedMesh* mesh, bool forceUpload = false);
    void setRenderMode(bool useMeshPreview);
    void setWireframeMode(bool useWireframe) { renderWireframe = useWireframe; }
    bool isRenderingMesh() const { return renderMeshPreview; }
    
    // Get the rendered texture for ImGui display
    GLuint getColorTexture() const { return colorTexture; }
    
private:
    // OpenGL resources - Raymarching
    GLuint quadVAO;
    GLuint quadVBO;
    std::unique_ptr<Shader> raymarchShader;
    
    // OpenGL resources - Mesh rendering
    GLuint meshVAO;
    GLuint meshVBO;
    GLuint meshEBO;
    std::unique_ptr<Shader> meshShader;
    const BakedMesh* currentMesh;
    size_t meshIndexCount;
    
    // Framebuffer for off-screen rendering
    GLuint framebuffer;
    GLuint colorTexture;
    GLuint depthTexture;
    
    // State
    bool initialized;
    bool renderMeshPreview;
    bool renderWireframe;
    
    // Private methods - Raymarching
    bool createQuadGeometry();
    bool loadShaders();
    void renderFullscreenQuad();
    void setShaderUniforms(const BakingConfig& config, float sizeRatio, float distanceRatio,
                          const CameraController& camera, const glm::vec2& viewportSize);
    
    // Private methods - Mesh rendering
    bool loadMeshShaders();
    void uploadMeshToGPU(const BakedMesh& mesh);
    void renderMesh(const CameraController& camera, const glm::vec2& viewportSize);
    void cleanupMeshBuffers();
    
    // Framebuffer management
    bool createFramebuffer(int width, int height);
    void resizeFramebuffer(int width, int height);
};