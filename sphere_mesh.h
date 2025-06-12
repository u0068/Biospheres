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

private:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    GLuint VAO, VBO, EBO;
    GLuint instanceVBO; // Buffer for instance data
    int indexCount;

public:
    SphereMesh();
    ~SphereMesh();
    
    void generateSphere(int latitudeSegments = 32, int longitudeSegments = 32, float radius = 1.0f);
    void setupBuffers();
    void setupInstanceBuffer(GLuint instanceDataBuffer);
    void render(int instanceCount) const;
    void cleanup();
    
    int getIndexCount() const { return indexCount; }
};
