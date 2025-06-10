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
              << indices.size() << " indices" << std::endl;
}

void SphereMesh::setupBuffers() {
    // Generate and bind VAO
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    
    // Generate and setup VBO for vertices
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
    
    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(1);
    
    // Generate and setup EBO
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    
    glBindVertexArray(0);
}

void SphereMesh::setupInstanceBuffer(GLuint instanceDataBuffer) {
    glBindVertexArray(VAO);
    
    // Bind the instance data buffer as an additional vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, instanceDataBuffer);
    
    // Instance position and radius (location = 2) - vec4
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(glm::vec4), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // This attribute advances once per instance
    
    glBindVertexArray(0);
}

void SphereMesh::render(int instanceCount) const {
    if (VAO == 0 || indexCount == 0) {
        std::cerr << "SphereMesh not properly initialized!" << std::endl;
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
