#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>

class SphereMesh {
public:
    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal;
    };

    // LOD level definitions
    static constexpr int LOD_LEVELS = 4;
    static constexpr int LOD_SEGMENTS[LOD_LEVELS] = {32, 16, 8, 4}; // Latitude/longitude segments for each LOD

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
    void setupBuffers();
    void setupLODBuffers(); // Setup buffers for all LOD levels
    void setupInstanceBuffer(GLuint instanceDataBuffer);
    void setupLODInstanceBuffer(GLuint lodInstanceDataBuffer); // Setup LOD instance buffer
    void render(int instanceCount) const;
    void renderLOD(int lodLevel, int instanceCount, int instanceOffset = 0) const; // Render specific LOD level
    void cleanup();
    
    int getIndexCount() const { return indexCount[0]; }
    int getLODIndexCount(int lodLevel) const { return lodLevel < LOD_LEVELS ? indexCount[lodLevel] : 0; }
};
