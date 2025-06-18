#include "sphere_mesh.h"
#include <cmath>
#include <iostream>

SphereMesh::SphereMesh() : VAO(0), VBO(0), EBO(0), instanceVBO(0), indexCount(0) {
}

SphereMesh::~SphereMesh() {
    cleanup();
}

void SphereMesh::generateSphere(int latitudeSegments, int longitudeSegments, float radius) {
    vertices.clear();
    indices.clear();
    
    const float PI = 3.14159265359f;
    
    // Generate vertices
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        float theta = lat * PI / latitudeSegments;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            float phi = lon * 2 * PI / longitudeSegments;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            Vertex vertex;
            vertex.position.x = radius * sinTheta * cosPhi;
            vertex.position.y = radius * cosTheta;
            vertex.position.z = radius * sinTheta * sinPhi;
            
            // Normal is the same as position for a unit sphere
            vertex.normal = glm::normalize(vertex.position);
            
            vertices.push_back(vertex);
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            int current = lat * (longitudeSegments + 1) + lon;
            int next = current + longitudeSegments + 1;
            
            // First triangle
            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);
            
            // Second triangle
            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }
    
    indexCount = static_cast<int>(indices.size());
    
    std::cout << "Generated sphere with " << vertices.size() << " vertices and " 
              << indices.size() << " indices\n";
}

void SphereMesh::setupBuffers() {
    glCreateVertexArrays(1, &VAO); // DSA way of creating VAO

    // Create and populate VBO with DSA
    glCreateBuffers(1, &VBO);
    glNamedBufferData(VBO, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Link VBO to VAO slot 0
    glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(Vertex));

    // Set attribute 0 (position) from slot 0
    glEnableVertexArrayAttrib(VAO, 0);
    glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
    glVertexArrayAttribBinding(VAO, 0, 0);

    // Set attribute 1 (normal) from slot 0
    glEnableVertexArrayAttrib(VAO, 1);
    glVertexArrayAttribFormat(VAO, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
    glVertexArrayAttribBinding(VAO, 1, 0);

    // Create and populate EBO (element/index buffer)
    glCreateBuffers(1, &EBO);
    glNamedBufferData(EBO, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    glVertexArrayElementBuffer(VAO, EBO); // Attach index buffer to VAO
}

void SphereMesh::setupInstanceBuffer(GLuint instanceDataBuffer) {
    // Associate instance buffer with slot 1
    glVertexArrayVertexBuffer(VAO, 1, instanceDataBuffer, 0, 2*sizeof(glm::vec4));

    // Attribute 2: positionAndRadius (offset 0)
    glEnableVertexArrayAttrib(VAO, 2);
    glVertexArrayAttribFormat(VAO, 2, 4, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(VAO, 2, 1);

    // Attribute 3: color (offset sizeof(vec4))
    glEnableVertexArrayAttrib(VAO, 3);
    glVertexArrayAttribFormat(VAO, 3, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4));
    glVertexArrayAttribBinding(VAO, 3, 1);

    // Set divisor to 1 for instancing
    glVertexArrayBindingDivisor(VAO, 1, 1);
}

void SphereMesh::render(int instanceCount) const {
    if (VAO == 0 || indexCount == 0) {
        std::cerr << "SphereMesh not properly initialized!\n";
        return;
    }
    
    glBindVertexArray(VAO);
    glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
    glBindVertexArray(0);
}

void SphereMesh::cleanup() {
    if (EBO != 0) {
        glDeleteBuffers(1, &EBO);
        EBO = 0;
    }
    if (VBO != 0) {
        glDeleteBuffers(1, &VBO);
        VBO = 0;
    }
    if (VAO != 0) {
        glDeleteVertexArrays(1, &VAO);
        VAO = 0;
    }
    
    vertices.clear();
    indices.clear();
    indexCount = 0;
}
