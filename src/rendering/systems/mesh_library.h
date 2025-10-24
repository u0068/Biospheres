#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <map>
#include <vector>
#include <array>

/**
 * MeshLibrary
 * 
 * Manages loading and storage of pre-baked bridge mesh variations.
 * Provides mesh query by parameters and bilinear interpolation mesh selection.
 * 
 * Requirements: 4.4, 12.1, 12.2
 */
class MeshLibrary {
public:
    /**
     * MeshVariation
     * 
     * Represents a single baked mesh variation with its GPU resources.
     * Each variation corresponds to specific size ratio and distance ratio parameters.
     */
    struct MeshVariation {
        GLuint vao;                 // Vertex Array Object
        GLuint vbo;                 // Vertex Buffer Object (positions + normals)
        GLuint ebo;                 // Element Buffer Object (indices)
        uint32_t indexCount;        // Number of indices
        float sizeRatio;            // Size ratio parameter
        float distanceRatio;        // Distance ratio parameter
        
        MeshVariation()
            : vao(0), vbo(0), ebo(0), indexCount(0)
            , sizeRatio(0.0f), distanceRatio(0.0f)
        {}
    };
    
    /**
     * InterpolationMeshes
     * 
     * Contains 4 mesh variations and their interpolation weights for bilinear interpolation.
     */
    struct InterpolationMeshes {
        std::array<const MeshVariation*, 4> meshes;  // 4 corner meshes
        std::array<float, 4> weights;                 // Interpolation weights
        bool isExactMatch;                            // True if exact match found
        
        InterpolationMeshes()
            : meshes{nullptr, nullptr, nullptr, nullptr}
            , weights{0.0f, 0.0f, 0.0f, 0.0f}
            , isExactMatch(false)
        {}
    };

    MeshLibrary();
    ~MeshLibrary();

    // Mesh loading (Requirement 4.4)
    bool loadMesh(const std::string& filepath);
    void loadMeshDirectory(const std::string& directory);
    
    // Mesh loading statistics (Requirement 4.4, 12.1)
    struct LoadingStats {
        int totalFiles;
        int loadedSuccessfully;
        int failedValidation;
        int failedLoading;
        std::vector<std::string> errorMessages;
        
        LoadingStats() : totalFiles(0), loadedSuccessfully(0), 
                        failedValidation(0), failedLoading(0) {}
    };
    
    const LoadingStats& getLastLoadingStats() const { return m_lastLoadingStats; }
    
    // Mesh query (Requirement 12.1)
    const MeshVariation* getMesh(float sizeRatio, float distanceRatio) const;
    const MeshVariation* getNearestMesh(float sizeRatio, float distanceRatio) const;
    
    // Bilinear interpolation mesh selection (Requirement 12.2)
    InterpolationMeshes getInterpolationMeshes(float sizeRatio, float distanceRatio) const;
    
    // Statistics
    size_t getMeshCount() const { return m_meshes.size(); }
    const std::vector<float>& getUniqueSizeRatios() const { return m_uniqueSizeRatios; }
    const std::vector<float>& getUniqueDistanceRatios() const { return m_uniqueDistanceRatios; }
    
    // Cleanup
    void clear();

private:
    /**
     * MeshKey
     * 
     * Key for storing meshes in map, based on size and distance ratios.
     * Uses integer representation for floating point comparison.
     */
    struct MeshKey {
        int sizeRatioKey;
        int distanceRatioKey;
        
        MeshKey(float sizeRatio, float distanceRatio);
        
        bool operator<(const MeshKey& other) const {
            if (sizeRatioKey != other.sizeRatioKey) {
                return sizeRatioKey < other.sizeRatioKey;
            }
            return distanceRatioKey < other.distanceRatioKey;
        }
        
        bool operator==(const MeshKey& other) const {
            return sizeRatioKey == other.sizeRatioKey && 
                   distanceRatioKey == other.distanceRatioKey;
        }
    };
    
    /**
     * MeshFileHeader
     * 
     * Binary mesh file header structure (matches baker tool output).
     */
    struct MeshFileHeader {
        uint32_t magic;              // 'MESH' = 0x4D455348
        uint32_t version;            // Format version (1)
        uint32_t vertexCount;        // Number of vertices
        uint32_t indexCount;         // Number of indices
        float sizeRatio;             // Size ratio parameter
        float distanceRatio;         // Distance ratio parameter
        uint32_t reserved1;          // Reserved for future use
        uint32_t reserved2;          // Reserved for future use
    };
    
    // Helper methods
    bool loadMeshFromFile(const std::string& filepath, MeshVariation& mesh);
    bool validateMeshFile(const MeshFileHeader& header) const;
    bool validateMeshTopology(const std::vector<glm::vec3>& positions,
                             const std::vector<glm::vec3>& normals,
                             const std::vector<uint32_t>& indices) const;
    bool parseFilenameParameters(const std::string& filename, float& sizeRatio, float& distanceRatio) const;
    void createVAO(MeshVariation& mesh, const std::vector<glm::vec3>& positions,
                   const std::vector<glm::vec3>& normals, const std::vector<uint32_t>& indices);
    void buildUniqueRatioLists();
    void findInterpolationCorners(float sizeRatio, float distanceRatio,
                                  float& s0, float& s1, float& d0, float& d1) const;
    float calculateParameterDistance(float sizeRatio1, float distanceRatio1,
                                     float sizeRatio2, float distanceRatio2) const;
    
    // Data storage
    std::map<MeshKey, MeshVariation> m_meshes;      // All loaded mesh variations
    std::vector<float> m_uniqueSizeRatios;          // Sorted unique size ratios
    std::vector<float> m_uniqueDistanceRatios;      // Sorted unique distance ratios
    LoadingStats m_lastLoadingStats;                // Statistics from last load operation
    
    // Constants
    static constexpr uint32_t MESH_MAGIC = 0x4D455348;  // 'MESH'
    static constexpr uint32_t MESH_VERSION = 1;
    static constexpr float EPSILON = 0.001f;             // For floating point comparison
};
