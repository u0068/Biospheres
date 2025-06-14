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

void SphereMesh::setupInstanceBuffer(BufferGroup instanceDataBuffers) {
    int attribLocation = 2; // Start at attribute location 2
    for (int i = 0; i < instanceDataBuffers.BUFFER_COUNT; ++i)
    {
        GLuint buffer = instanceDataBuffers.buffers[i];
        int bindingIndex = 1 + i; // Avoid slot 0, used by mesh VBO
        int typeSize = instanceDataBuffers.dataTypeSizes[i];
    	int numComponents = typeSize / sizeof(float);
        assert(numComponents >= 1 && numComponents <= 4); // Required by OpenGL

        // Assume all are vec4 for now (adjust later if needed)
        glVertexArrayVertexBuffer(VAO, bindingIndex, buffer, 0, typeSize);

        glEnableVertexArrayAttrib(VAO, attribLocation);
        glVertexArrayAttribFormat(VAO, attribLocation, numComponents, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(VAO, attribLocation, bindingIndex);
        glVertexArrayBindingDivisor(VAO, bindingIndex, 1); // Mark as per-instance

        ++attribLocation;
    }
}

void SphereMesh::render(int instanceCount) const {
    assert(VAO != 0);
    assert(indexCount > 0);
    assert(glIsVertexArray(VAO));
    assert(glIsBuffer(EBO));
    
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
