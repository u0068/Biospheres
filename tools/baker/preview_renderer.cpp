#include "preview_renderer.h"
#include "baker_ui.h"
#include "camera_controller.h"
#include "mesh_baker.h"
#include "shader.h"

// Standard includes
#include <iostream>

PreviewRenderer::PreviewRenderer()
    : quadVAO(0)
    , quadVBO(0)
    , meshVAO(0)
    , meshVBO(0)
    , meshEBO(0)
    , currentMesh(nullptr)
    , meshIndexCount(0)
    , framebuffer(0)
    , colorTexture(0)
    , depthTexture(0)
    , initialized(false)
    , renderMeshPreview(false)
    , renderWireframe(false)
{
}

PreviewRenderer::~PreviewRenderer()
{
    cleanup();
}

bool PreviewRenderer::initialize()
{
    if (initialized)
    {
        return true;
    }
    
    try
    {
        // Create fullscreen quad geometry
        if (!createQuadGeometry())
        {
            std::cerr << "Failed to create quad geometry\n";
            return false;
        }
        
        // Load raymarching shader
        if (!loadShaders())
        {
            std::cerr << "Failed to load shaders\n";
            return false;
        }
        
        // Load mesh rendering shader
        if (!loadMeshShaders())
        {
            std::cerr << "Failed to load mesh shaders\n";
            return false;
        }
        
        // Create framebuffer for off-screen rendering
        if (!createFramebuffer(800, 600)) // Initial size
        {
            std::cerr << "Failed to create framebuffer\n";
            return false;
        }
        
        initialized = true;
        std::cout << "Preview renderer initialized successfully\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during preview renderer initialization: " << e.what() << "\n";
        return false;
    }
}

void PreviewRenderer::cleanup()
{
    cleanupMeshBuffers();
    
    if (depthTexture != 0)
    {
        glDeleteTextures(1, &depthTexture);
        depthTexture = 0;
    }
    
    if (colorTexture != 0)
    {
        glDeleteTextures(1, &colorTexture);
        colorTexture = 0;
    }
    
    if (framebuffer != 0)
    {
        glDeleteFramebuffers(1, &framebuffer);
        framebuffer = 0;
    }
    
    if (quadVBO != 0)
    {
        glDeleteBuffers(1, &quadVBO);
        quadVBO = 0;
    }
    
    if (quadVAO != 0)
    {
        glDeleteVertexArrays(1, &quadVAO);
        quadVAO = 0;
    }
    
    raymarchShader.reset();
    initialized = false;
}

void PreviewRenderer::render(const BakingConfig& config, float sizeRatio, float distanceRatio,
                           const CameraController& camera, const glm::vec2& viewportSize)
{
    if (!initialized)
    {
        return;
    }
    
    // Resize framebuffer if needed
    if (viewportSize.x > 0 && viewportSize.y > 0)
    {
        resizeFramebuffer(static_cast<int>(viewportSize.x), static_cast<int>(viewportSize.y));
    }
    
    // Bind framebuffer for off-screen rendering
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, static_cast<int>(viewportSize.x), static_cast<int>(viewportSize.y));
    
    // Clear framebuffer
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Render based on mode
    static bool lastRenderMeshPreview = false;
    if (renderMeshPreview != lastRenderMeshPreview)
    {
        std::cout << "Render mode changed: " << (renderMeshPreview ? "MESH" : "RAYMARCHING") << "\n";
        std::cout << "  renderMeshPreview: " << renderMeshPreview << "\n";
        std::cout << "  currentMesh: " << (currentMesh ? "valid" : "null") << "\n";
        std::cout << "  meshVAO: " << meshVAO << "\n";
        lastRenderMeshPreview = renderMeshPreview;
    }
    
    if (renderMeshPreview && currentMesh && meshVAO != 0)
    {
        // Render mesh preview
        renderMesh(camera, viewportSize);
    }
    else if (raymarchShader)
    {
        // Use raymarching shader
        raymarchShader->use();
        
        // Set shader uniforms
        setShaderUniforms(config, sizeRatio, distanceRatio, camera, viewportSize);
        
        // Render fullscreen quad
        renderFullscreenQuad();
    }
    
    // Unbind framebuffer (back to default)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool PreviewRenderer::createQuadGeometry()
{
    // Fullscreen quad vertices (NDC coordinates)
    float quadVertices[] = {
        // positions   // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };
    
    // Generate and bind VAO
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    
    glBindVertexArray(quadVAO);
    
    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    return true;
}

bool PreviewRenderer::loadShaders()
{
    try
    {
        // Load raymarching shaders (paths should be relative to project root after working directory is set)
        raymarchShader = std::make_unique<Shader>("shaders/baker/raymarching.vert", "shaders/baker/raymarching.frag");
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load raymarching shader: " << e.what() << "\n";
        return false;
    }
}

void PreviewRenderer::renderFullscreenQuad()
{
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void PreviewRenderer::setShaderUniforms(const BakingConfig& config, float sizeRatio, float distanceRatio,
                                       const CameraController& camera, const glm::vec2& viewportSize)
{
    if (!raymarchShader)
    {
        return;
    }
    
    // Camera matrices
    glm::mat4 viewMatrix = camera.getViewMatrix();
    glm::mat4 projMatrix = camera.getProjectionMatrix(viewportSize);
    glm::mat4 invViewMatrix = glm::inverse(viewMatrix);
    glm::mat4 invProjMatrix = glm::inverse(projMatrix);
    
    // Set uniforms
    raymarchShader->setMat4("u_invViewMatrix", invViewMatrix);
    raymarchShader->setMat4("u_invProjMatrix", invProjMatrix);
    raymarchShader->setVec3("u_cameraPos", camera.getPosition());
    raymarchShader->setVec2("u_resolution", viewportSize);
    
    // Metaball parameters
    raymarchShader->setFloat("u_sizeRatio", sizeRatio);
    raymarchShader->setFloat("u_distanceRatio", distanceRatio);
    
    // SDF blending parameters
    raymarchShader->setFloat("u_blendingStrength", config.blendingStrength);
    
    // Pass curve control points (up to 8)
    int numPoints = std::min(static_cast<int>(config.blendCurvePoints.size()), 8);
    raymarchShader->setInt("u_numCurvePoints", numPoints);
    
    for (int i = 0; i < numPoints; ++i)
    {
        std::string distName = "u_curveDistances[" + std::to_string(i) + "]";
        std::string multName = "u_curveMultipliers[" + std::to_string(i) + "]";
        raymarchShader->setFloat(distName, config.blendCurvePoints[i].distanceRatio);
        raymarchShader->setFloat(multName, config.blendCurvePoints[i].blendMultiplier);
    }
    
    // Raymarching parameters
    raymarchShader->setInt("u_maxSteps", 128);
    raymarchShader->setFloat("u_maxDistance", 100.0f);
    raymarchShader->setFloat("u_epsilon", 0.001f);
}

bool PreviewRenderer::createFramebuffer(int width, int height)
{
    // Generate framebuffer
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    
    // Create color texture
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    
    // Create depth texture
    glGenTextures(1, &depthTexture);
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    
    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Framebuffer not complete!\n";
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void PreviewRenderer::resizeFramebuffer(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    
    // Resize color texture
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    
    // Resize depth texture
    glBindTexture(GL_TEXTURE_2D, depthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Mesh preview methods

void PreviewRenderer::setPreviewMesh(const BakedMesh* mesh, bool forceUpload)
{
    // Skip if same mesh and not forcing upload
    if (!forceUpload && currentMesh == mesh && meshVAO != 0)
    {
        return; // Same mesh and already uploaded
    }
    
    currentMesh = mesh;
    
    if (mesh && mesh->isValid())
    {
        // Upload if forced, no mesh exists, or mesh size changed
        if (forceUpload || meshVAO == 0 || meshIndexCount != mesh->indices.size())
        {
            std::cout << "PreviewRenderer: Setting valid mesh with " << mesh->positions.size() << " vertices\n";
            uploadMeshToGPU(*mesh);
        }
    }
    else
    {
        if (mesh)
        {
            std::cout << "PreviewRenderer: Mesh provided but invalid\n";
        }
        else if (meshVAO != 0)
        {
            std::cout << "PreviewRenderer: Clearing mesh (null pointer)\n";
        }
        cleanupMeshBuffers();
    }
}

void PreviewRenderer::setRenderMode(bool useMeshPreview)
{
    if (renderMeshPreview != useMeshPreview)
    {
        std::cout << "PreviewRenderer: Switching render mode to " 
                  << (useMeshPreview ? "MESH" : "RAYMARCHING") << "\n";
        renderMeshPreview = useMeshPreview;
    }
}

bool PreviewRenderer::loadMeshShaders()
{
    try
    {
        meshShader = std::make_unique<Shader>("shaders/baker/mesh_preview.vert", "shaders/baker/mesh_preview.frag");
        std::cout << "Mesh preview shaders loaded successfully\n";
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to load mesh preview shaders: " << e.what() << "\n";
        return false;
    }
}

void PreviewRenderer::uploadMeshToGPU(const BakedMesh& mesh)
{
    // Clean up existing buffers
    cleanupMeshBuffers();
    
    // Create VAO
    glGenVertexArrays(1, &meshVAO);
    glBindVertexArray(meshVAO);
    
    // Create VBO for positions and normals
    glGenBuffers(1, &meshVBO);
    glBindBuffer(GL_ARRAY_BUFFER, meshVBO);
    
    // Interleave position and normal data
    std::vector<float> vertexData;
    vertexData.reserve(mesh.positions.size() * 6); // 3 for position, 3 for normal
    
    for (size_t i = 0; i < mesh.positions.size(); ++i)
    {
        vertexData.push_back(mesh.positions[i].x);
        vertexData.push_back(mesh.positions[i].y);
        vertexData.push_back(mesh.positions[i].z);
        vertexData.push_back(mesh.normals[i].x);
        vertexData.push_back(mesh.normals[i].y);
        vertexData.push_back(mesh.normals[i].z);
    }
    
    glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), vertexData.data(), GL_STATIC_DRAW);
    
    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Create EBO
    glGenBuffers(1, &meshEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(uint32_t), mesh.indices.data(), GL_STATIC_DRAW);
    
    meshIndexCount = mesh.indices.size();
    
    glBindVertexArray(0);
    
    std::cout << "Uploaded mesh to GPU: " << mesh.positions.size() << " vertices, " 
              << (meshIndexCount / 3) << " triangles\n";
}

void PreviewRenderer::renderMesh(const CameraController& camera, const glm::vec2& viewportSize)
{
    if (!meshShader || meshVAO == 0 || meshIndexCount == 0)
    {
        return;
    }
    
    glEnable(GL_DEPTH_TEST);
    
    // Set polygon mode based on wireframe setting
    if (renderWireframe)
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glLineWidth(1.5f);
    }
    else
    {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    }
    
    meshShader->use();
    
    // Set matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 projection = camera.getProjectionMatrix(viewportSize);
    
    meshShader->setMat4("u_model", model);
    meshShader->setMat4("u_view", view);
    meshShader->setMat4("u_projection", projection);
    
    // Set lighting
    glm::vec3 lightDir = glm::normalize(glm::vec3(1.0f, 1.0f, 1.0f));
    meshShader->setVec3("u_lightDir", lightDir);
    meshShader->setVec3("u_viewPos", camera.getPosition());
    
    // Pure white for both modes
    glm::vec3 meshColor = glm::vec3(1.0f, 1.0f, 1.0f);
    meshShader->setVec3("u_meshColor", meshColor);
    
    // Render mesh
    glBindVertexArray(meshVAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(meshIndexCount), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    // Reset OpenGL state
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
}

void PreviewRenderer::cleanupMeshBuffers()
{
    if (meshEBO != 0)
    {
        glDeleteBuffers(1, &meshEBO);
        meshEBO = 0;
    }
    
    if (meshVBO != 0)
    {
        glDeleteBuffers(1, &meshVBO);
        meshVBO = 0;
    }
    
    if (meshVAO != 0)
    {
        glDeleteVertexArrays(1, &meshVAO);
        meshVAO = 0;
    }
    
    currentMesh = nullptr;
    meshIndexCount = 0;
}
