#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

class SphereMesh {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv; // Added UV coordinates
    };

    // LOD level definitions
    static constexpr int LOD_LEVELS = 4;
    static constexpr int LOD_SEGMENTS[LOD_LEVELS] = {32, 16, 8, 4}; // DEPRECATED: Was used for latitude/longitude spheres, LOD system now uses icospheres

private:
    // Per-LOD mesh data
    std::vector<Vertex> vertices[LOD_LEVELS];
    std::vector<unsigned int> indices[LOD_LEVELS];
    GLuint VAO[LOD_LEVELS], VBO[LOD_LEVELS], EBO[LOD_LEVELS];
    GLuint instanceVBO[LOD_LEVELS]; // Instance buffer for each LOD
    int indexCount[LOD_LEVELS];

public:
    SphereMesh();
    ~SphereMesh();
    
    void generateSphere(int latitudeSegments = 32, int longitudeSegments = 32, float radius = 1.0f);
    void generateLODSpheres(float radius = 1.0f); // Generate all LOD levels
    void generateIcosphere(int lod, int subdivisions, float radius);
    void setupBuffers();
    void setupLODBuffers(); // Setup buffers for all LOD levels
    void setupInstanceBuffer(GLuint instanceDataBuffer);
    void setupDistanceFadeInstanceBuffer(GLuint instanceDataBuffer); // Setup instance buffer with fade factor
    void setupLODInstanceBuffer(GLuint lodInstanceDataBuffer); // Setup LOD instance buffer
    void setupLODInstanceBuffers(GLuint lodInstanceBuffers[4]); // Setup separate instance buffers for each LOD level
    void setupLODInstanceBufferWithFade(int lodLevel, GLuint lodInstanceDataBuffer); // Setup LOD instance buffer with fade factor
    void render(int instanceCount) const;
    void renderLOD(int lodLevel, int instanceCount, int instanceOffset = 0) const; // Render specific LOD level
    void cleanup();
    
    int getIndexCount() const { return indexCount[0]; }
    int getLODIndexCount(int lodLevel) const { return lodLevel < LOD_LEVELS ? indexCount[lodLevel] : 0; }
};
