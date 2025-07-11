#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "../../rendering/core/shader_class.h"

// Forward declarations
class Camera;
class UIManager;

struct CellGizmoSystem
{
    // Gizmo orientation visualization
    GLuint gizmoBuffer{};           // Buffer for gizmo line vertices
    GLuint gizmoVAO{};              // VAO for gizmo rendering
    GLuint gizmoVBO{};              // VBO for gizmo vertices
    Shader* gizmoExtractShader = nullptr; // Compute shader for generating gizmo data
    Shader* gizmoShader = nullptr;        // Vertex/fragment shaders for rendering gizmos
    
    // Ring gizmo visualization
    GLuint ringGizmoBuffer{};           // Buffer for ring gizmo vertices
    GLuint ringGizmoVAO{};              // VAO for ring gizmo rendering
    GLuint ringGizmoVBO{};              // VBO for ring gizmo vertices
    Shader* ringGizmoExtractShader = nullptr; // Compute shader for generating ring gizmo data
    Shader* ringGizmoShader = nullptr;        // Vertex/fragment shaders for rendering ring gizmos
    
    // Adhesion line visualization
    GLuint adhesionLineBuffer{};        // Buffer for adhesionSettings line vertices  
    GLuint adhesionLineVAO{};           // VAO for adhesionSettings line rendering
    GLuint adhesionLineVBO{};           // VBO for adhesionSettings line vertices
    Shader* adhesionLineExtractShader = nullptr; // Generate adhesionSettings line data
    Shader* adhesionLineShader = nullptr;        // Vertex/fragment shaders for rendering adhesionSettings lines

    // Adhesion connection system
    GLuint adhesionConnectionBuffer{};  // Buffer storing permanent adhesionSettings connections
    Shader* adhesionPhysicsShader = nullptr;  // Compute shader for processing adhesionSettings physics

    // Constructor and destructor
    CellGizmoSystem();
    ~CellGizmoSystem();

    // Gizmo management functions
    void initializeGizmoBuffers();
    void updateGizmoData();
    void cleanupGizmos();
    void renderGizmos(glm::vec2 resolution, const Camera& camera, bool showGizmos);
    
    // Ring gizmo methods
    void renderRingGizmos(glm::vec2 resolution, const class Camera &camera, const class UIManager &uiManager);
    void initializeRingGizmoBuffers();
    void updateRingGizmoData();
    void cleanupRingGizmos();
    
    // Adhesion line methods
    void renderAdhesionLines(glm::vec2 resolution, const class Camera &camera, bool showAdhesionLines);
    void initializeAdhesionLineBuffers();
    void updateAdhesionLineData();
    void cleanupAdhesionLines();

    // Adhesion connection system
    void initializeAdhesionConnectionSystem();
    void runAdhesionPhysics();
    void cleanupAdhesionConnectionSystem();
}; 