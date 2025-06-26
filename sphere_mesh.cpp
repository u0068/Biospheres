#include "sphere_mesh.h"
#include <cmath>
#include <iostream>

SphereMesh::SphereMesh() {
    // Initialize arrays
    for (int i = 0; i < LOD_LEVELS; i++) {
        VAO[i] = VBO[i] = EBO[i] = instanceVBO[i] = 0;
        indexCount[i] = 0;
    }
}

SphereMesh::~SphereMesh() {
    cleanup();
}

void SphereMesh::generateSphere(int latitudeSegments, int longitudeSegments, float radius) {
    // Legacy function - generate only LOD 0
    vertices[0].clear();
    indices[0].clear();
    
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
            
            vertices[0].push_back(vertex);
        }
    }
    
    // Generate indices
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            int current = lat * (longitudeSegments + 1) + lon;
            int next = current + longitudeSegments + 1;
            
            // First triangle
            indices[0].push_back(current);
            indices[0].push_back(next);
            indices[0].push_back(current + 1);
            
            // Second triangle
            indices[0].push_back(current + 1);
            indices[0].push_back(next);
            indices[0].push_back(next + 1);
        }
    }
    
    indexCount[0] = static_cast<int>(indices[0].size());
}

void SphereMesh::generateLODSpheres(float radius) {
    const float PI = 3.14159265359f;
    
    // Generate spheres for all LOD levels
    for (int lod = 0; lod < LOD_LEVELS; lod++) {
        vertices[lod].clear();
        indices[lod].clear();
        
        int latSegments = LOD_SEGMENTS[lod];
        int lonSegments = LOD_SEGMENTS[lod];
        
        // Generate vertices for this LOD
        for (int lat = 0; lat <= latSegments; ++lat) {
            float theta = lat * PI / latSegments;
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);
            
            for (int lon = 0; lon <= lonSegments; ++lon) {
                float phi = lon * 2 * PI / lonSegments;
                float sinPhi = sin(phi);
                float cosPhi = cos(phi);
                
                Vertex vertex;
                vertex.position.x = radius * sinTheta * cosPhi;
                vertex.position.y = radius * cosTheta;
                vertex.position.z = radius * sinTheta * sinPhi;
                
                vertex.normal = glm::normalize(vertex.position);
                vertices[lod].push_back(vertex);
            }
        }
        
        // Generate indices for this LOD
        for (int lat = 0; lat < latSegments; ++lat) {
            for (int lon = 0; lon < lonSegments; ++lon) {
                int current = lat * (lonSegments + 1) + lon;
                int next = current + lonSegments + 1;
                
                // First triangle
                indices[lod].push_back(current);
                indices[lod].push_back(next);
                indices[lod].push_back(current + 1);
                
                // Second triangle
                indices[lod].push_back(current + 1);
                indices[lod].push_back(next);
                indices[lod].push_back(next + 1);
            }
        }
        
        indexCount[lod] = static_cast<int>(indices[lod].size());
    }
}

void SphereMesh::setupBuffers() {
    // Legacy function - setup only LOD 0
    if (vertices[0].empty() || indices[0].empty()) {
        std::cerr << "Error: Cannot setup buffers with empty vertex/index data\n";
        return;
    }

    glGenVertexArrays(1, &VAO[0]);
    glGenBuffers(1, &VBO[0]);
    glGenBuffers(1, &EBO[0]);

    glBindVertexArray(VAO[0]);

    // Vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, VBO[0]);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices[0].size() * sizeof(Vertex)), vertices[0].data(), GL_STATIC_DRAW);

    // Element buffer
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[0]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices[0].size() * sizeof(unsigned int)), indices[0].data(), GL_STATIC_DRAW);

    // Vertex attributes
    // Position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    
    // Normal
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

    glBindVertexArray(0);
}

void SphereMesh::setupLODBuffers() {
    for (int lod = 0; lod < LOD_LEVELS; lod++) {
        if (vertices[lod].empty() || indices[lod].empty()) {
            std::cerr << "Error: Cannot setup LOD " << lod << " buffers with empty vertex/index data\n";
            continue;
        }

        glGenVertexArrays(1, &VAO[lod]);
        glGenBuffers(1, &VBO[lod]);
        glGenBuffers(1, &EBO[lod]);

        glBindVertexArray(VAO[lod]);

        // Vertex buffer
        glBindBuffer(GL_ARRAY_BUFFER, VBO[lod]);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices[lod].size() * sizeof(Vertex)), vertices[lod].data(), GL_STATIC_DRAW);

        // Element buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO[lod]);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices[lod].size() * sizeof(unsigned int)), indices[lod].data(), GL_STATIC_DRAW);

        // Vertex attributes
        // Position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        
        // Normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        glBindVertexArray(0);
    }
}

void SphereMesh::setupInstanceBuffer(GLuint instanceDataBuffer) {
    // Legacy function - setup only for LOD 0
    instanceVBO[0] = instanceDataBuffer;
    
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, instanceDataBuffer);

    // Instance position and radius (vec4)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glVertexAttribDivisor(2, 1);

    // Instance color (vec4)
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(3, 1);

    // Instance orientation (vec4 quaternion)
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
}

void SphereMesh::setupLODInstanceBuffer(GLuint instanceDataBuffer) {
    // Setup instance buffer for all LOD levels (using standard instance format)
    for (int lod = 0; lod < LOD_LEVELS; lod++) {
        instanceVBO[lod] = instanceDataBuffer;
        
        glBindVertexArray(VAO[lod]);
        glBindBuffer(GL_ARRAY_BUFFER, instanceDataBuffer);

        // Standard instance data structure: positionAndRadius (vec4), color (vec4), orientation (vec4)
        size_t stride = 12 * sizeof(float); // 3 vec4s = 12 floats

        // Instance position and radius (vec4)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(2, 1);

        // Instance color (vec4)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

        // Instance orientation (vec4 quaternion)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);
    }
}

void SphereMesh::setupLODInstanceBuffers(GLuint lodInstanceBuffers[4]) {
    // Setup separate instance buffer for each LOD level
    for (int lod = 0; lod < LOD_LEVELS; lod++) {
        instanceVBO[lod] = lodInstanceBuffers[lod];
        
        glBindVertexArray(VAO[lod]);
        glBindBuffer(GL_ARRAY_BUFFER, lodInstanceBuffers[lod]);

        // Standard instance data structure: positionAndRadius (vec4), color (vec4), orientation (vec4)
        size_t stride = 12 * sizeof(float); // 3 vec4s = 12 floats

        // Instance position and radius (vec4)
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
        glVertexAttribDivisor(2, 1);

        // Instance color (vec4)
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        glVertexAttribDivisor(3, 1);

        // Instance orientation (vec4 quaternion)
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, (void*)(8 * sizeof(float)));
        glVertexAttribDivisor(4, 1);

        glBindVertexArray(0);
    }
}

void SphereMesh::render(int instanceCount) const {
    if (VAO[0] == 0 || indexCount[0] == 0 || instanceCount <= 0) return;

    glBindVertexArray(VAO[0]);
    glDrawElementsInstanced(GL_TRIANGLES, indexCount[0], GL_UNSIGNED_INT, 0, instanceCount);
    glBindVertexArray(0);
}

void SphereMesh::renderLOD(int lodLevel, int instanceCount, int instanceOffset) const {
    if (lodLevel < 0 || lodLevel >= LOD_LEVELS || VAO[lodLevel] == 0 || 
        indexCount[lodLevel] == 0 || instanceCount <= 0) return;

    glBindVertexArray(VAO[lodLevel]);
    
    if (instanceOffset > 0) {
        // Use glDrawElementsInstancedBaseInstance for offset rendering
        glDrawElementsInstancedBaseInstance(GL_TRIANGLES, indexCount[lodLevel], GL_UNSIGNED_INT, 
                                          0, instanceCount, instanceOffset);
    } else {
        glDrawElementsInstanced(GL_TRIANGLES, indexCount[lodLevel], GL_UNSIGNED_INT, 0, instanceCount);
    }
    
    glBindVertexArray(0);
}

void SphereMesh::cleanup() {
    for (int lod = 0; lod < LOD_LEVELS; lod++) {
        if (EBO[lod] != 0) {
            glDeleteBuffers(1, &EBO[lod]);
            EBO[lod] = 0;
        }
        if (VBO[lod] != 0) {
            glDeleteBuffers(1, &VBO[lod]);
            VBO[lod] = 0;
        }
        if (VAO[lod] != 0) {
            glDeleteVertexArrays(1, &VAO[lod]);
            VAO[lod] = 0;
        }
        
        vertices[lod].clear();
        indices[lod].clear();
        indexCount[lod] = 0;
    }
}
