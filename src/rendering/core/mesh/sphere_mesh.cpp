#include "sphere_mesh.h"
#include <cmath>
#include <iostream>
#include <map>

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

// Helper function to get midpoint and cache it
static unsigned int getMidpoint(unsigned int v1, unsigned int v2, std::vector<SphereMesh::Vertex>& vertices, std::map<std::pair<unsigned int, unsigned int>, unsigned int>& midpointCache, float radius) {
    auto key = std::make_pair(std::min(v1, v2), std::max(v1, v2));
    if (midpointCache.count(key)) return midpointCache[key];
    SphereMesh::Vertex& vert1 = vertices[v1];
    SphereMesh::Vertex& vert2 = vertices[v2];
    SphereMesh::Vertex mid;
    mid.position = glm::normalize((vert1.position + vert2.position) * 0.5f) * radius;
    mid.normal = glm::normalize(mid.position);
    // Spherical mapping for UVs
    float u = 0.5f + atan2(mid.position.z, mid.position.x) / (2.0f * 3.14159265359f);
    float v = 0.5f - asin(mid.position.y / radius) / 3.14159265359f;
    mid.uv = glm::vec2(u, v);
    vertices.push_back(mid);
    unsigned int idx = static_cast<unsigned int>(vertices.size() - 1);
    midpointCache[key] = idx;
    return idx;
}

void SphereMesh::generateIcosphere(int lod, int subdivisions, float radius) {
    vertices[lod].clear();
    indices[lod].clear();
    // Golden ratio
    const float t = (1.0f + sqrt(5.0f)) / 2.0f;
    // Create 12 vertices of a icosahedron
    std::vector<SphereMesh::Vertex> verts = {
        {{-1,  t,  0}, {}, {}}, {{ 1,  t,  0}, {}, {}}, {{-1, -t,  0}, {}, {}}, {{ 1, -t,  0}, {}, {}},
        {{ 0, -1,  t}, {}, {}}, {{ 0,  1,  t}, {}, {}}, {{ 0, -1, -t}, {}, {}}, {{ 0,  1, -t}, {}, {}},
        {{ t,  0, -1}, {}, {}}, {{ t,  0,  1}, {}, {}}, {{-t,  0, -1}, {}, {}}, {{-t,  0,  1}, {}, {}}
    };
    // Normalize and assign normals/UVs
    for (auto& v : verts) {
        v.position = glm::normalize(v.position) * radius;
        v.normal = glm::normalize(v.position);
        float u = 0.5f + atan2(v.position.z, v.position.x) / (2.0f * 3.14159265359f);
        float v_uv = 0.5f - asin(v.position.y / radius) / 3.14159265359f;
        v.uv = glm::vec2(u, v_uv);
    }
    vertices[lod] = verts;
    // 20 faces of the icosahedron
    std::vector<unsigned int> faces = {
        0,11,5, 0,5,1, 0,1,7, 0,7,10, 0,10,11,
        1,5,9, 5,11,4, 11,10,2, 10,7,6, 7,1,8,
        3,9,4, 3,4,2, 3,2,6, 3,6,8, 3,8,9,
        4,9,5, 2,4,11, 6,2,10, 8,6,7, 9,8,1
    };
    indices[lod] = faces;
    // Subdivide faces
    for (int i = 0; i < subdivisions; ++i) {
        std::map<std::pair<unsigned int, unsigned int>, unsigned int> midpointCache;
        std::vector<unsigned int> newFaces;
        for (size_t f = 0; f < indices[lod].size(); f += 3) {
            unsigned int v1 = indices[lod][f];
            unsigned int v2 = indices[lod][f+1];
            unsigned int v3 = indices[lod][f+2];
            unsigned int a = getMidpoint(v1, v2, vertices[lod], midpointCache, radius);
            unsigned int b = getMidpoint(v2, v3, vertices[lod], midpointCache, radius);
            unsigned int c = getMidpoint(v3, v1, vertices[lod], midpointCache, radius);
            newFaces.insert(newFaces.end(), {v1, a, c, v2, b, a, v3, c, b, a, b, c});
        }
        indices[lod] = newFaces;
    }
    indexCount[lod] = static_cast<int>(indices[lod].size());
}

void SphereMesh::generateSphere(int latitudeSegments, int longitudeSegments, float radius) {
    vertices[0].clear();
    indices[0].clear();
    
    // Generate vertices
    for (int lat = 0; lat <= latitudeSegments; ++lat) {
        float theta = 3.14159265358979323846f * float(lat) / float(latitudeSegments);
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);
        
        for (int lon = 0; lon <= longitudeSegments; ++lon) {
            float phi = 2.0f * 3.14159265358979323846f * float(lon) / float(longitudeSegments);
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            
            Vertex vertex;
            
            // Position
            vertex.position.x = radius * sinTheta * cosPhi;
            vertex.position.y = radius * cosTheta;
            vertex.position.z = radius * sinTheta * sinPhi;
            
            // Normal (same as position for unit sphere, normalized for radius)
            vertex.normal = glm::normalize(vertex.position);
            
            // UV coordinates
            vertex.uv.x = float(lon) / float(longitudeSegments);
            vertex.uv.y = float(lat) / float(latitudeSegments);
            
            vertices[0].push_back(vertex);
        }
    }
    
    // Generate indices for triangles with correct winding order (counter-clockwise for outward-facing normals)
    for (int lat = 0; lat < latitudeSegments; ++lat) {
        for (int lon = 0; lon < longitudeSegments; ++lon) {
            int current = lat * (longitudeSegments + 1) + lon;
            int next = current + longitudeSegments + 1;
            
            // First triangle (counter-clockwise winding for outward-facing normals)
            indices[0].push_back(current);
            indices[0].push_back(current + 1);
            indices[0].push_back(next);
            
            // Second triangle (counter-clockwise winding for outward-facing normals)
            indices[0].push_back(current + 1);
            indices[0].push_back(next + 1);
            indices[0].push_back(next);
        }
    }
    
    indexCount[0] = static_cast<int>(indices[0].size());
}

void SphereMesh::generateLODSpheres(float radius) {
    // Use icosphere for all LODs with decreasing subdivisions
    int lod_subdivs[LOD_LEVELS] = {3, 2, 1, 0};
    for (int lod = 0; lod < LOD_LEVELS; ++lod) {
        generateIcosphere(lod, lod_subdivs[lod], radius);
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

    // UV
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

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

        // UV
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

        glBindVertexArray(0);
    }
}

void SphereMesh::setupInstanceBuffer(GLuint instanceDataBuffer) {
    // Legacy function - setup only for LOD 0
    instanceVBO[0] = instanceDataBuffer;
    
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, instanceDataBuffer);

    // Instance position and radius (vec4) - Location 3 to match shader
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)0);
    glVertexAttribDivisor(3, 1);

    // Instance color (vec4) - Location 4 to match shader
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(4 * sizeof(float)));
    glVertexAttribDivisor(4, 1);

    // Instance orientation (vec4 quaternion) - Location 5 to match shader
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, 12 * sizeof(float), (void*)(8 * sizeof(float)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void SphereMesh::setupDistanceFadeInstanceBuffer(GLuint instanceDataBuffer) {
    // Setup instance buffer for distance fade format (positionAndRadius, color, orientation, fadeFactor)
    instanceVBO[0] = instanceDataBuffer;
    
    glBindVertexArray(VAO[0]);
    glBindBuffer(GL_ARRAY_BUFFER, instanceDataBuffer);

    // Instance data structure: positionAndRadius (vec4), color (vec4), orientation (vec4), fadeFactor (vec4)
    size_t stride = 16 * sizeof(float); // 4 vec4s = 16 floats

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

    // Instance fade factor (vec4 - only x component used)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    glVertexAttribDivisor(5, 1);

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

void SphereMesh::setupLODInstanceBufferWithFade(int lodLevel, GLuint lodInstanceDataBuffer) {
    // Setup instance buffer for specific LOD level with fade factor
    if (lodLevel < 0 || lodLevel >= LOD_LEVELS) {
        std::cerr << "Error: Invalid LOD level " << lodLevel << " in setupLODInstanceBufferWithFade\n";
        return;
    }
    
    instanceVBO[lodLevel] = lodInstanceDataBuffer;
    
    glBindVertexArray(VAO[lodLevel]);
    glBindBuffer(GL_ARRAY_BUFFER, lodInstanceDataBuffer);

    // Instance data structure: positionAndRadius (vec4), color (vec4), orientation (vec4), fadeFactor (vec4)
    size_t stride = 16 * sizeof(float); // 4 vec4s = 16 floats

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

    // Instance fade factor (vec4 - only x component used)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, (void*)(12 * sizeof(float)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
}

void SphereMesh::render(int instanceCount) const {
    if (VAO[0] == 0 || indexCount[0] == 0 || instanceCount <= 0) return;

    glBindVertexArray(VAO[0]);
    glDrawElementsInstanced(GL_TRIANGLES, indexCount[0], GL_UNSIGNED_INT, 0, instanceCount);
    glBindVertexArray(0);
}

void SphereMesh::renderSingle() const {
    if (VAO[0] == 0 || indexCount[0] == 0) return;

    glBindVertexArray(VAO[0]);
    glDrawElements(GL_TRIANGLES, indexCount[0], GL_UNSIGNED_INT, 0);
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
