#include "mesh_library.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

MeshLibrary::MeshLibrary()
{
}

MeshLibrary::~MeshLibrary()
{
    clear();
}

// ============================================================================
// MESHKEY IMPLEMENTATION
// ============================================================================

MeshLibrary::MeshKey::MeshKey(float sizeRatio, float distanceRatio)
{
    // Convert floats to integer keys for reliable comparison
    // Multiply by 1000 to preserve 3 decimal places
    sizeRatioKey = static_cast<int>(std::round(sizeRatio * 1000.0f));
    distanceRatioKey = static_cast<int>(std::round(distanceRatio * 1000.0f));
}

// ============================================================================
// MESH LOADING (Requirement 4.4)
// ============================================================================

bool MeshLibrary::loadMesh(const std::string& filepath)
{
    MeshVariation mesh;
    
    if (!loadMeshFromFile(filepath, mesh)) {
        std::cerr << "MeshLibrary: Failed to load mesh from " << filepath << std::endl;
        return false;
    }
    
    // Store mesh in map
    MeshKey key(mesh.sizeRatio, mesh.distanceRatio);
    m_meshes[key] = mesh;
    
    // Rebuild unique ratio lists
    buildUniqueRatioLists();
    
    std::cout << "MeshLibrary: Loaded mesh (size=" << mesh.sizeRatio 
              << ", distance=" << mesh.distanceRatio 
              << ", vertices=" << mesh.indexCount / 3 << " triangles)" << std::endl;
    
    return true;
}

void MeshLibrary::loadMeshDirectory(const std::string& directory)
{
    if (!std::filesystem::exists(directory)) {
        std::cerr << "MeshLibrary: Directory does not exist: " << directory << std::endl;
        return;
    }
    
    int loadedCount = 0;
    int failedCount = 0;
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".mesh") {
            if (loadMesh(entry.path().string())) {
                loadedCount++;
            } else {
                failedCount++;
            }
        }
    }
    
    std::cout << "MeshLibrary: Loaded " << loadedCount << " meshes from " << directory;
    if (failedCount > 0) {
        std::cout << " (" << failedCount << " failed)";
    }
    std::cout << std::endl;
}

bool MeshLibrary::loadMeshFromFile(const std::string& filepath, MeshVariation& mesh)
{
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "MeshLibrary: Failed to open file: " << filepath << std::endl;
        return false;
    }
    
    // Read header
    MeshFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(MeshFileHeader));
    
    if (!file.good()) {
        std::cerr << "MeshLibrary: Failed to read header from " << filepath << std::endl;
        return false;
    }
    
    // Validate header
    if (!validateMeshFile(header)) {
        std::cerr << "MeshLibrary: Invalid mesh file: " << filepath << std::endl;
        return false;
    }
    
    // Read vertex positions
    std::vector<glm::vec3> positions(header.vertexCount);
    file.read(reinterpret_cast<char*>(positions.data()), 
              header.vertexCount * sizeof(glm::vec3));
    
    if (!file.good()) {
        std::cerr << "MeshLibrary: Failed to read positions from " << filepath << std::endl;
        return false;
    }
    
    // Read vertex normals
    std::vector<glm::vec3> normals(header.vertexCount);
    file.read(reinterpret_cast<char*>(normals.data()), 
              header.vertexCount * sizeof(glm::vec3));
    
    if (!file.good()) {
        std::cerr << "MeshLibrary: Failed to read normals from " << filepath << std::endl;
        return false;
    }
    
    // Read triangle indices
    std::vector<uint32_t> indices(header.indexCount);
    file.read(reinterpret_cast<char*>(indices.data()), 
              header.indexCount * sizeof(uint32_t));
    
    if (!file.good()) {
        std::cerr << "MeshLibrary: Failed to read indices from " << filepath << std::endl;
        return false;
    }
    
    file.close();
    
    // Store parameters
    mesh.sizeRatio = header.sizeRatio;
    mesh.distanceRatio = header.distanceRatio;
    mesh.indexCount = header.indexCount;
    
    // Create VAO and upload to GPU
    createVAO(mesh, positions, normals, indices);
    
    return true;
}

bool MeshLibrary::validateMeshFile(const MeshFileHeader& header) const
{
    // Check magic number
    if (header.magic != MESH_MAGIC) {
        std::cerr << "MeshLibrary: Invalid magic number: 0x" << std::hex << header.magic 
                  << " (expected 0x" << MESH_MAGIC << ")" << std::dec << std::endl;
        return false;
    }
    
    // Check version
    if (header.version != MESH_VERSION) {
        std::cerr << "MeshLibrary: Unsupported version: " << header.version 
                  << " (expected " << MESH_VERSION << ")" << std::endl;
        return false;
    }
    
    // Validate vertex count
    if (header.vertexCount == 0) {
        std::cerr << "MeshLibrary: Invalid vertex count: 0" << std::endl;
        return false;
    }
    
    // Validate index count (must be multiple of 3 for triangles)
    if (header.indexCount == 0 || header.indexCount % 3 != 0) {
        std::cerr << "MeshLibrary: Invalid index count: " << header.indexCount << std::endl;
        return false;
    }
    
    // Validate size ratio (must be >= 1.0)
    if (header.sizeRatio < 1.0f || header.sizeRatio > 10.0f) {
        std::cerr << "MeshLibrary: Invalid size ratio: " << header.sizeRatio << std::endl;
        return false;
    }
    
    // Validate distance ratio (must be > 0)
    if (header.distanceRatio <= 0.0f || header.distanceRatio > 3.0f) {
        std::cerr << "MeshLibrary: Invalid distance ratio: " << header.distanceRatio << std::endl;
        return false;
    }
    
    return true;
}

void MeshLibrary::createVAO(MeshVariation& mesh, const std::vector<glm::vec3>& positions,
                            const std::vector<glm::vec3>& normals, const std::vector<uint32_t>& indices)
{
    // Generate VAO
    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);
    
    // Create VBO for interleaved position and normal data
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    
    // Interleave positions and normals for better cache coherency
    std::vector<float> interleavedData;
    interleavedData.reserve(positions.size() * 6);  // 3 floats position + 3 floats normal
    
    for (size_t i = 0; i < positions.size(); ++i) {
        // Position
        interleavedData.push_back(positions[i].x);
        interleavedData.push_back(positions[i].y);
        interleavedData.push_back(positions[i].z);
        
        // Normal
        interleavedData.push_back(normals[i].x);
        interleavedData.push_back(normals[i].y);
        interleavedData.push_back(normals[i].z);
    }
    
    glBufferData(GL_ARRAY_BUFFER, interleavedData.size() * sizeof(float), 
                 interleavedData.data(), GL_STATIC_DRAW);
    
    // Set up vertex attributes
    // Position attribute (location = 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    // Normal attribute (location = 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 
                         (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    // Create EBO
    glGenBuffers(1, &mesh.ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), 
                 indices.data(), GL_STATIC_DRAW);
    
    // Unbind VAO
    glBindVertexArray(0);
}

// ============================================================================
// MESH INDEXING SYSTEM (Requirement 12.1)
// ============================================================================

void MeshLibrary::buildUniqueRatioLists()
{
    m_uniqueSizeRatios.clear();
    m_uniqueDistanceRatios.clear();
    
    // Collect unique ratios
    for (const auto& [key, mesh] : m_meshes) {
        m_uniqueSizeRatios.push_back(mesh.sizeRatio);
        m_uniqueDistanceRatios.push_back(mesh.distanceRatio);
    }
    
    // Sort and remove duplicates
    std::sort(m_uniqueSizeRatios.begin(), m_uniqueSizeRatios.end());
    std::sort(m_uniqueDistanceRatios.begin(), m_uniqueDistanceRatios.end());
    
    m_uniqueSizeRatios.erase(
        std::unique(m_uniqueSizeRatios.begin(), m_uniqueSizeRatios.end(),
                   [](float a, float b) { return std::abs(a - b) < EPSILON; }),
        m_uniqueSizeRatios.end()
    );
    
    m_uniqueDistanceRatios.erase(
        std::unique(m_uniqueDistanceRatios.begin(), m_uniqueDistanceRatios.end(),
                   [](float a, float b) { return std::abs(a - b) < EPSILON; }),
        m_uniqueDistanceRatios.end()
    );
}

const MeshLibrary::MeshVariation* MeshLibrary::getMesh(float sizeRatio, float distanceRatio) const
{
    MeshKey key(sizeRatio, distanceRatio);
    auto it = m_meshes.find(key);
    
    if (it != m_meshes.end()) {
        return &it->second;
    }
    
    return nullptr;
}

const MeshLibrary::MeshVariation* MeshLibrary::getNearestMesh(float sizeRatio, float distanceRatio) const
{
    if (m_meshes.empty()) {
        return nullptr;
    }
    
    // Find nearest mesh by parameter distance
    const MeshVariation* nearest = nullptr;
    float minDistance = std::numeric_limits<float>::max();
    
    for (const auto& [key, mesh] : m_meshes) {
        float distance = calculateParameterDistance(sizeRatio, distanceRatio,
                                                    mesh.sizeRatio, mesh.distanceRatio);
        if (distance < minDistance) {
            minDistance = distance;
            nearest = &mesh;
        }
    }
    
    return nearest;
}

// ============================================================================
// BILINEAR INTERPOLATION (Requirement 12.2)
// ============================================================================

MeshLibrary::InterpolationMeshes MeshLibrary::getInterpolationMeshes(
    float sizeRatio, float distanceRatio) const
{
    InterpolationMeshes result;
    
    // Check for exact match first
    const MeshVariation* exactMatch = getMesh(sizeRatio, distanceRatio);
    if (exactMatch) {
        result.meshes[0] = exactMatch;
        result.weights[0] = 1.0f;
        result.isExactMatch = true;
        return result;
    }
    
    // Find interpolation corners
    float s0, s1, d0, d1;
    findInterpolationCorners(sizeRatio, distanceRatio, s0, s1, d0, d1);
    
    // Get 4 corner meshes
    result.meshes[0] = getMesh(s0, d0);  // Bottom-left
    result.meshes[1] = getMesh(s1, d0);  // Bottom-right
    result.meshes[2] = getMesh(s0, d1);  // Top-left
    result.meshes[3] = getMesh(s1, d1);  // Top-right
    
    // Calculate bilinear interpolation weights
    float sWeight = 0.0f;
    float dWeight = 0.0f;
    
    if (std::abs(s1 - s0) > EPSILON) {
        sWeight = (sizeRatio - s0) / (s1 - s0);
    }
    
    if (std::abs(d1 - d0) > EPSILON) {
        dWeight = (distanceRatio - d0) / (d1 - d0);
    }
    
    // Clamp weights to [0, 1]
    sWeight = std::clamp(sWeight, 0.0f, 1.0f);
    dWeight = std::clamp(dWeight, 0.0f, 1.0f);
    
    // Calculate bilinear weights
    result.weights[0] = (1.0f - sWeight) * (1.0f - dWeight);  // Bottom-left
    result.weights[1] = sWeight * (1.0f - dWeight);           // Bottom-right
    result.weights[2] = (1.0f - sWeight) * dWeight;           // Top-left
    result.weights[3] = sWeight * dWeight;                    // Top-right
    
    result.isExactMatch = false;
    
    return result;
}

void MeshLibrary::findInterpolationCorners(float sizeRatio, float distanceRatio,
                                           float& s0, float& s1, float& d0, float& d1) const
{
    // Find size ratio bounds
    if (m_uniqueSizeRatios.empty()) {
        s0 = s1 = sizeRatio;
    } else if (sizeRatio <= m_uniqueSizeRatios.front()) {
        s0 = s1 = m_uniqueSizeRatios.front();
    } else if (sizeRatio >= m_uniqueSizeRatios.back()) {
        s0 = s1 = m_uniqueSizeRatios.back();
    } else {
        // Find surrounding values
        auto upper = std::upper_bound(m_uniqueSizeRatios.begin(), m_uniqueSizeRatios.end(), sizeRatio);
        s1 = *upper;
        s0 = *(upper - 1);
    }
    
    // Find distance ratio bounds
    if (m_uniqueDistanceRatios.empty()) {
        d0 = d1 = distanceRatio;
    } else if (distanceRatio <= m_uniqueDistanceRatios.front()) {
        d0 = d1 = m_uniqueDistanceRatios.front();
    } else if (distanceRatio >= m_uniqueDistanceRatios.back()) {
        d0 = d1 = m_uniqueDistanceRatios.back();
    } else {
        // Find surrounding values
        auto upper = std::upper_bound(m_uniqueDistanceRatios.begin(), 
                                     m_uniqueDistanceRatios.end(), distanceRatio);
        d1 = *upper;
        d0 = *(upper - 1);
    }
}

float MeshLibrary::calculateParameterDistance(float sizeRatio1, float distanceRatio1,
                                              float sizeRatio2, float distanceRatio2) const
{
    // Euclidean distance in parameter space
    float ds = sizeRatio1 - sizeRatio2;
    float dd = distanceRatio1 - distanceRatio2;
    return std::sqrt(ds * ds + dd * dd);
}

// ============================================================================
// CLEANUP
// ============================================================================

void MeshLibrary::clear()
{
    // Delete all GPU resources
    for (auto& [key, mesh] : m_meshes) {
        if (mesh.vao != 0) {
            glDeleteVertexArrays(1, &mesh.vao);
        }
        if (mesh.vbo != 0) {
            glDeleteBuffers(1, &mesh.vbo);
        }
        if (mesh.ebo != 0) {
            glDeleteBuffers(1, &mesh.ebo);
        }
    }
    
    m_meshes.clear();
    m_uniqueSizeRatios.clear();
    m_uniqueDistanceRatios.clear();
}
