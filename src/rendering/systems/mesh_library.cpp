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
    // Reset loading statistics (Requirement 4.4)
    m_lastLoadingStats = LoadingStats();
    
    // Validate directory exists
    if (!std::filesystem::exists(directory)) {
        std::string error = "Directory does not exist: " + directory;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.errorMessages.push_back(error);
        return;
    }
    
    if (!std::filesystem::is_directory(directory)) {
        std::string error = "Path is not a directory: " + directory;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.errorMessages.push_back(error);
        return;
    }
    
    std::cout << "MeshLibrary: Scanning directory: " << directory << std::endl;
    
    // Collect all .mesh files (Requirement 4.4)
    std::vector<std::filesystem::path> meshFiles;
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
            if (entry.is_regular_file() && entry.path().extension() == ".mesh") {
                meshFiles.push_back(entry.path());
                m_lastLoadingStats.totalFiles++;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::string error = "Filesystem error while scanning: " + std::string(e.what());
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.errorMessages.push_back(error);
        return;
    }
    
    if (meshFiles.empty()) {
        std::cout << "MeshLibrary: No .mesh files found in " << directory << std::endl;
        return;
    }
    
    std::cout << "MeshLibrary: Found " << meshFiles.size() << " .mesh files" << std::endl;
    
    // Sort files for consistent loading order
    std::sort(meshFiles.begin(), meshFiles.end());
    
    // Load each mesh file with progress reporting (Requirement 4.4)
    int progressInterval = std::max(1, static_cast<int>(meshFiles.size()) / 10);
    
    for (size_t i = 0; i < meshFiles.size(); ++i) {
        const auto& filepath = meshFiles[i];
        
        // Report progress every 10%
        if ((i + 1) % progressInterval == 0 || i == meshFiles.size() - 1) {
            float progress = (static_cast<float>(i + 1) / meshFiles.size()) * 100.0f;
            std::cout << "MeshLibrary: Loading progress: " << static_cast<int>(progress) 
                      << "% (" << (i + 1) << "/" << meshFiles.size() << ")" << std::endl;
        }
        
        // Attempt to load mesh
        if (loadMesh(filepath.string())) {
            m_lastLoadingStats.loadedSuccessfully++;
        }
        // Error tracking is handled in loadMesh() and loadMeshFromFile()
    }
    
    // Report final statistics (Requirement 4.4)
    std::cout << "\n=== Mesh Loading Summary ===" << std::endl;
    std::cout << "Total files found: " << m_lastLoadingStats.totalFiles << std::endl;
    std::cout << "Loaded successfully: " << m_lastLoadingStats.loadedSuccessfully << std::endl;
    std::cout << "Failed validation: " << m_lastLoadingStats.failedValidation << std::endl;
    std::cout << "Failed loading: " << m_lastLoadingStats.failedLoading << std::endl;
    
    if (!m_lastLoadingStats.errorMessages.empty()) {
        std::cout << "\nErrors encountered:" << std::endl;
        for (const auto& error : m_lastLoadingStats.errorMessages) {
            std::cout << "  - " << error << std::endl;
        }
    }
    
    std::cout << "===========================\n" << std::endl;
}

bool MeshLibrary::loadMeshFromFile(const std::string& filepath, MeshVariation& mesh)
{
    // Open file (Requirement 4.4)
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::string error = "Failed to open file: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedLoading++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Get file size for validation
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Validate minimum file size
    if (fileSize < static_cast<std::streamsize>(sizeof(MeshFileHeader))) {
        std::string error = "File too small to contain valid header: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedValidation++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Read header (Requirement 12.1)
    MeshFileHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(MeshFileHeader));
    
    if (!file.good()) {
        std::string error = "Failed to read header from: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedLoading++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Validate header (Requirement 12.1)
    if (!validateMeshFile(header)) {
        std::string error = "Invalid mesh file header: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedValidation++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Validate expected file size
    size_t expectedSize = sizeof(MeshFileHeader) +
                         header.vertexCount * sizeof(glm::vec3) * 2 +  // positions + normals
                         header.indexCount * sizeof(uint32_t);
    
    if (fileSize < static_cast<std::streamsize>(expectedSize)) {
        std::string error = "File size mismatch (expected " + std::to_string(expectedSize) + 
                          " bytes, got " + std::to_string(fileSize) + "): " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedValidation++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Read vertex positions (Requirement 4.4)
    std::vector<glm::vec3> positions(header.vertexCount);
    file.read(reinterpret_cast<char*>(positions.data()), 
              header.vertexCount * sizeof(glm::vec3));
    
    if (!file.good()) {
        std::string error = "Failed to read positions from: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedLoading++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Read vertex normals (Requirement 4.4)
    std::vector<glm::vec3> normals(header.vertexCount);
    file.read(reinterpret_cast<char*>(normals.data()), 
              header.vertexCount * sizeof(glm::vec3));
    
    if (!file.good()) {
        std::string error = "Failed to read normals from: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedLoading++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Read triangle indices (Requirement 4.4)
    std::vector<uint32_t> indices(header.indexCount);
    file.read(reinterpret_cast<char*>(indices.data()), 
              header.indexCount * sizeof(uint32_t));
    
    if (!file.good()) {
        std::string error = "Failed to read indices from: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedLoading++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    file.close();
    
    // Validate mesh topology (Requirement 12.1)
    if (!validateMeshTopology(positions, normals, indices)) {
        std::string error = "Mesh topology validation failed: " + filepath;
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedValidation++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    // Store parameters
    mesh.sizeRatio = header.sizeRatio;
    mesh.distanceRatio = header.distanceRatio;
    mesh.indexCount = header.indexCount;
    
    // Create VAO and upload to GPU (Requirement 4.4)
    try {
        createVAO(mesh, positions, normals, indices);
    } catch (const std::exception& e) {
        std::string error = "Failed to create VAO for: " + filepath + " - " + e.what();
        std::cerr << "MeshLibrary: " << error << std::endl;
        m_lastLoadingStats.failedLoading++;
        m_lastLoadingStats.errorMessages.push_back(error);
        return false;
    }
    
    return true;
}

bool MeshLibrary::validateMeshFile(const MeshFileHeader& header) const
{
    // Check magic number (Requirement 12.1)
    if (header.magic != MESH_MAGIC) {
        std::cerr << "MeshLibrary: Invalid magic number: 0x" << std::hex << header.magic 
                  << " (expected 0x" << MESH_MAGIC << ")" << std::dec << std::endl;
        return false;
    }
    
    // Check version (Requirement 12.1)
    if (header.version != MESH_VERSION) {
        std::cerr << "MeshLibrary: Unsupported version: " << header.version 
                  << " (expected " << MESH_VERSION << ")" << std::endl;
        return false;
    }
    
    // Validate vertex count (Requirement 12.1)
    if (header.vertexCount == 0) {
        std::cerr << "MeshLibrary: Invalid vertex count: 0" << std::endl;
        return false;
    }
    
    // Validate reasonable vertex count (prevent memory issues)
    if (header.vertexCount > 10000000) {  // 10 million vertices max
        std::cerr << "MeshLibrary: Vertex count too large: " << header.vertexCount << std::endl;
        return false;
    }
    
    // Validate index count (must be multiple of 3 for triangles) (Requirement 12.1)
    if (header.indexCount == 0 || header.indexCount % 3 != 0) {
        std::cerr << "MeshLibrary: Invalid index count: " << header.indexCount << std::endl;
        return false;
    }
    
    // Validate reasonable index count
    if (header.indexCount > 30000000) {  // 30 million indices max (10 million triangles)
        std::cerr << "MeshLibrary: Index count too large: " << header.indexCount << std::endl;
        return false;
    }
    
    // Validate size ratio (must be >= 1.0) (Requirement 12.1)
    if (header.sizeRatio < 1.0f || header.sizeRatio > 10.0f) {
        std::cerr << "MeshLibrary: Invalid size ratio: " << header.sizeRatio << std::endl;
        return false;
    }
    
    // Validate distance ratio (must be > 0) (Requirement 12.1)
    if (header.distanceRatio <= 0.0f || header.distanceRatio > 3.0f) {
        std::cerr << "MeshLibrary: Invalid distance ratio: " << header.distanceRatio << std::endl;
        return false;
    }
    
    return true;
}

bool MeshLibrary::validateMeshTopology(const std::vector<glm::vec3>& positions,
                                       const std::vector<glm::vec3>& normals,
                                       const std::vector<uint32_t>& indices) const
{
    // Validate data sizes match (Requirement 12.1)
    if (positions.size() != normals.size()) {
        std::cerr << "MeshLibrary: Position count (" << positions.size() 
                  << ") does not match normal count (" << normals.size() << ")" << std::endl;
        return false;
    }
    
    // Validate all indices are within bounds (Requirement 12.1)
    uint32_t maxIndex = static_cast<uint32_t>(positions.size());
    for (size_t i = 0; i < indices.size(); ++i) {
        if (indices[i] >= maxIndex) {
            std::cerr << "MeshLibrary: Index out of bounds at position " << i 
                      << ": " << indices[i] << " (max: " << maxIndex - 1 << ")" << std::endl;
            return false;
        }
    }
    
    // Validate normals are normalized (with tolerance) (Requirement 12.1)
    int invalidNormalCount = 0;
    for (size_t i = 0; i < normals.size(); ++i) {
        float length = glm::length(normals[i]);
        if (length < 0.9f || length > 1.1f) {
            invalidNormalCount++;
            if (invalidNormalCount <= 5) {  // Only report first 5
                std::cerr << "MeshLibrary: Warning - Normal at index " << i 
                          << " has invalid length: " << length << std::endl;
            }
        }
    }
    
    if (invalidNormalCount > 0) {
        std::cerr << "MeshLibrary: Warning - " << invalidNormalCount 
                  << " normals have invalid length (expected ~1.0)" << std::endl;
        // Don't fail on this, just warn
    }
    
    // Check for degenerate triangles (Requirement 12.1)
    int degenerateCount = 0;
    for (size_t i = 0; i < indices.size(); i += 3) {
        uint32_t i0 = indices[i];
        uint32_t i1 = indices[i + 1];
        uint32_t i2 = indices[i + 2];
        
        // Check for duplicate indices in triangle
        if (i0 == i1 || i1 == i2 || i2 == i0) {
            degenerateCount++;
            if (degenerateCount <= 5) {  // Only report first 5
                std::cerr << "MeshLibrary: Warning - Degenerate triangle at index " << i 
                          << ": [" << i0 << ", " << i1 << ", " << i2 << "]" << std::endl;
            }
        }
    }
    
    if (degenerateCount > 0) {
        std::cerr << "MeshLibrary: Warning - " << degenerateCount 
                  << " degenerate triangles found" << std::endl;
        // Don't fail on this, just warn
    }
    
    // Check for NaN or infinite values in positions (Requirement 12.1)
    for (size_t i = 0; i < positions.size(); ++i) {
        const glm::vec3& pos = positions[i];
        if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z) ||
            std::isinf(pos.x) || std::isinf(pos.y) || std::isinf(pos.z)) {
            std::cerr << "MeshLibrary: Invalid position at index " << i 
                      << ": [" << pos.x << ", " << pos.y << ", " << pos.z << "]" << std::endl;
            return false;
        }
    }
    
    // Check for NaN or infinite values in normals (Requirement 12.1)
    for (size_t i = 0; i < normals.size(); ++i) {
        const glm::vec3& normal = normals[i];
        if (std::isnan(normal.x) || std::isnan(normal.y) || std::isnan(normal.z) ||
            std::isinf(normal.x) || std::isinf(normal.y) || std::isinf(normal.z)) {
            std::cerr << "MeshLibrary: Invalid normal at index " << i 
                      << ": [" << normal.x << ", " << normal.y << ", " << normal.z << "]" << std::endl;
            return false;
        }
    }
    
    return true;
}

bool MeshLibrary::parseFilenameParameters(const std::string& filename, 
                                         float& sizeRatio, float& distanceRatio) const
{
    // Expected format: bridge_r{sizeRatio}_d{distanceRatio}.mesh
    // Example: bridge_r1.5_d2.0.mesh
    
    size_t rPos = filename.find("_r");
    size_t dPos = filename.find("_d");
    
    if (rPos == std::string::npos || dPos == std::string::npos) {
        return false;
    }
    
    try {
        // Extract size ratio
        size_t rStart = rPos + 2;
        size_t rEnd = filename.find('_', rStart);
        if (rEnd == std::string::npos) {
            return false;
        }
        std::string sizeStr = filename.substr(rStart, rEnd - rStart);
        sizeRatio = std::stof(sizeStr);
        
        // Extract distance ratio
        size_t dStart = dPos + 2;
        size_t dEnd = filename.find('.', dStart);
        if (dEnd == std::string::npos) {
            return false;
        }
        std::string distStr = filename.substr(dStart, dEnd - dStart);
        distanceRatio = std::stof(distStr);
        
        return true;
    } catch (const std::exception&) {
        return false;
    }
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
