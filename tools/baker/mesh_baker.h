#pragma once

// Third-party includes
#include <glm/glm.hpp>

// Standard includes
#include <vector>
#include <string>
#include <functional>

// Forward declarations
struct BakingConfig;

/**
 * Blend curve control point
 */
struct BlendCurvePoint {
    float distanceRatio;
    float blendMultiplier;
};

/**
 * Structure representing a baked mesh
 */
struct BakedMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
    
    // Metadata
    float sizeRatio;
    float distanceRatio;
    float baseBlendingStrength;
    std::vector<BlendCurvePoint> blendCurve;
    
    // Validation
    bool isValid() const {
        return !positions.empty() && 
               positions.size() == normals.size() && 
               !indices.empty() && 
               (indices.size() % 3) == 0;
    }
};

/**
 * Core baking engine using raymarching and marching cubes
 * Generates metaball geometry from SDF evaluation
 */
class MeshBaker {
public:
    MeshBaker() = default;
    ~MeshBaker() = default;
    
    // Single mesh baking
    BakedMesh bakeDumbbellMesh(float sizeRatio, float distanceRatio, int resolution, 
                               float baseBlendingStrength, const std::vector<BlendCurvePoint>& blendCurve);
    
    // Batch baking
    // Progress callback returns true to continue, false to cancel
    void bakeAllVariations(
        const BakingConfig& config,
        std::function<bool(float, const std::string&)> progressCallback
    );
    
    // SDF evaluation
    float evaluateSDF(const glm::vec3& pos, float sizeRatio, float distance, 
                     float baseBlendingStrength, const std::vector<BlendCurvePoint>& blendCurve);
    
    // Curve metadata I/O
    void writeCurveMetadata(const std::string& outputDirectory, float baseBlendingStrength, 
                           const std::vector<BlendCurvePoint>& blendCurve);
    static std::vector<BlendCurvePoint> loadCurveMetadata(const std::string& filepath, float& outBaseBlendingStrength);
    
private:
    // SDF functions
    float sdSphere(const glm::vec3& pos, float radius);
    float softMin(float a, float b, float k = 0.5f);
    
    // Curve interpolation
    float interpolateBlendMultiplier(float distanceRatio, const std::vector<BlendCurvePoint>& curve) const;
    float catmullRomInterpolate(float t, float p0, float p1, float p2, float p3) const;
    
    // Marching cubes
    BakedMesh marchingCubes(const std::vector<float>& densityField, int resolution);
    
    // File I/O
    void writeMeshToBinary(const BakedMesh& mesh, const std::string& filepath);
    BakedMesh readMeshFromBinary(const std::string& filepath);
    bool validateMeshFile(const std::string& filepath);
    std::string generateFilename(float sizeRatio, float distanceRatio);
    
    // Utility
    glm::vec3 calculateGradient(const std::vector<float>& field, int x, int y, int z, int resolution);
    int getFieldIndex(int x, int y, int z, int resolution);
};