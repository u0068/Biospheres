#include "mesh_baker.h"
#include "baker_ui.h"
#include "marching_cubes_tables.h"

// Standard includes
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <unordered_map>

BakedMesh MeshBaker::bakeDumbbellMesh(float sizeRatio, float distanceRatio, int resolution,
                                     float baseBlendingStrength, const std::vector<BlendCurvePoint>& blendCurve)
{
    std::cout << "Baking mesh: sizeRatio=" << sizeRatio << ", distanceRatio=" << distanceRatio 
              << ", resolution=" << resolution << "^3\n";
    
    BakedMesh mesh;
    mesh.sizeRatio = sizeRatio;
    mesh.distanceRatio = distanceRatio;
    mesh.baseBlendingStrength = baseBlendingStrength;
    mesh.blendCurve = blendCurve;
    
    try
    {
        // Calculate actual distance from ratio
        float smallRadius = 1.0f; // Normalized mesh
        float largeRadius = smallRadius * sizeRatio;
        float combinedRadius = smallRadius + largeRadius;
        float distance = distanceRatio * combinedRadius;
        
        // Generate density field
        std::vector<float> densityField;
        densityField.reserve(resolution * resolution * resolution);
        
        // Calculate bounds to ensure we capture the full shape
        float maxExtent = distance * 0.5f + largeRadius + 0.5f; // Add padding
        float step = (maxExtent * 2.0f) / (resolution - 1);
        float offset = -maxExtent;
        
        for (int z = 0; z < resolution; ++z)
        {
            for (int y = 0; y < resolution; ++y)
            {
                for (int x = 0; x < resolution; ++x)
                {
                    glm::vec3 pos(
                        offset + x * step,
                        offset + y * step,
                        offset + z * step
                    );
                    
                    float sdf = evaluateSDF(pos, sizeRatio, distance, baseBlendingStrength, blendCurve);
                    densityField.push_back(-sdf); // Negative for marching cubes (inside = positive)
                }
            }
        }
        
        // Extract mesh using marching cubes
        mesh = marchingCubes(densityField, resolution);
        mesh.sizeRatio = sizeRatio;
        mesh.distanceRatio = distanceRatio;
        mesh.baseBlendingStrength = baseBlendingStrength;
        mesh.blendCurve = blendCurve;
        
        std::cout << "Generated mesh with " << mesh.positions.size() << " vertices, " 
                  << (mesh.indices.size() / 3) << " triangles\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception during mesh baking: " << e.what() << "\n";
    }
    
    return mesh;
}

void MeshBaker::bakeAllVariations(const BakingConfig& config, 
                                 std::function<bool(float, const std::string&)> progressCallback)
{
    int totalMeshes = config.getTotalMeshCount();
    int currentMesh = 0;
    bool cancelled = false;
    
    std::cout << "Starting batch baking of " << totalMeshes << " mesh variations\n";
    
    // Show current working directory for debugging
    std::filesystem::path currentPath = std::filesystem::current_path();
    std::cout << "Current working directory: " << currentPath.string() << "\n";
    
    // Create output directory if it doesn't exist
    try
    {
        std::cout << "Creating output directory: " << config.outputDirectory << "\n";
        
        // Convert to absolute path to see what we're actually trying to create
        std::filesystem::path targetPath = std::filesystem::absolute(config.outputDirectory);
        std::cout << "Absolute target path: " << targetPath.string() << "\n";
        
        std::filesystem::create_directories(config.outputDirectory);
        
        // Verify directory was created
        if (!std::filesystem::exists(config.outputDirectory))
        {
            throw std::runtime_error("Failed to create output directory: " + config.outputDirectory);
        }
        
        // Get absolute path for confirmation
        std::filesystem::path absPath = std::filesystem::absolute(config.outputDirectory);
        std::cout << "Output directory created/verified: " << absPath.string() << "\n";
    }
    catch (const std::exception& e)
    {
        std::cerr << "ERROR: Failed to create output directory: " << e.what() << "\n";
        if (progressCallback)
        {
            progressCallback(0.0f, "ERROR: Failed to create output directory");
        }
        return;
    }
    
    // Write curve metadata file once for the entire batch
    writeCurveMetadata(config.outputDirectory, config.blendingStrength, config.sizeRatioCurves);
    std::cout << "Wrote blend curve metadata with " << config.sizeRatioCurves.size() << " size ratio curves\n";
    
    // Iterate through all size ratios
    for (float sizeRatio : config.sizeRatios)
    {
        // Check for cancellation
        if (cancelled)
        {
            break;
        }
        
        // Iterate through all distance ratios
        for (float distanceRatio : config.distanceRatios)
        {
            // Update progress
            float progress = static_cast<float>(currentMesh) / totalMeshes;
            std::string status = "Baking: Size Ratio " + std::to_string(sizeRatio) + 
                               " - Distance " + std::to_string(distanceRatio) + 
                               " (" + std::to_string(currentMesh + 1) + "/" + std::to_string(totalMeshes) + ")";
            
            // Call progress callback and check for cancellation
            if (progressCallback)
            {
                bool shouldContinue = progressCallback(progress, status);
                if (!shouldContinue)
                {
                    std::cout << "Baking cancelled by user\n";
                    cancelled = true;
                    break;
                }
            }
            
            try
            {
                // Get the curve for this specific size ratio
                std::vector<BlendCurvePoint> curveForThisRatio = getCurveForSizeRatio(sizeRatio, config.sizeRatioCurves);
                
                // Bake mesh with curve data
                BakedMesh mesh = bakeDumbbellMesh(sizeRatio, distanceRatio, config.resolution,
                                                 config.blendingStrength, curveForThisRatio);
                
                if (mesh.isValid())
                {
                    // Generate filename and write to disk
                    std::string filename = generateFilename(sizeRatio, distanceRatio);
                    std::string filepath = config.outputDirectory + filename;
                    
                    writeMeshToBinary(mesh, filepath);
                    std::cout << "Wrote mesh: " << filename << "\n";
                }
                else
                {
                    std::cerr << "Generated invalid mesh for sizeRatio=" << sizeRatio 
                              << ", distanceRatio=" << distanceRatio << "\n";
                }
            }
            catch (const std::exception& e)
            {
                std::cerr << "Error baking mesh (sizeRatio=" << sizeRatio 
                          << ", distanceRatio=" << distanceRatio << "): " << e.what() << "\n";
            }
            
            currentMesh++;
        }
    }
    
    // Final progress update
    if (progressCallback && !cancelled)
    {
        progressCallback(1.0f, "Baking complete!");
    }
    
    if (cancelled)
    {
        std::cout << "Batch baking cancelled. Generated " << currentMesh << " of " << totalMeshes << " meshes.\n";
    }
    else
    {
        std::cout << "Batch baking completed. Generated " << currentMesh << " meshes.\n";
    }
}

float MeshBaker::evaluateSDF(const glm::vec3& pos, float sizeRatio, float distance,
                            float baseBlendingStrength, const std::vector<BlendCurvePoint>& blendCurve)
{
    float smallRadius = 1.0f;
    float largeRadius = smallRadius * sizeRatio;
    float combinedRadius = smallRadius + largeRadius;
    float distanceRatio = distance / combinedRadius;
    
    // Position spheres
    glm::vec3 leftCenter(-distance * 0.5f, 0.0f, 0.0f);
    glm::vec3 rightCenter(distance * 0.5f, 0.0f, 0.0f);
    
    // Calculate SDF for each sphere
    float leftSphere = sdSphere(pos - leftCenter, smallRadius);
    float rightSphere = sdSphere(pos - rightCenter, largeRadius);
    
    // Calculate adaptive blending strength using curve
    float blendMultiplier = interpolateBlendMultiplier(distanceRatio, blendCurve);
    float adaptiveBlendingStrength = baseBlendingStrength * blendMultiplier;
    adaptiveBlendingStrength = std::max(0.1f, std::min(10.0f, adaptiveBlendingStrength));
    
    // Blend with soft minimum
    return softMin(leftSphere, rightSphere, adaptiveBlendingStrength);
}

float MeshBaker::sdSphere(const glm::vec3& pos, float radius)
{
    return glm::length(pos) - radius;
}

float MeshBaker::softMin(float a, float b, float k)
{
    float h = std::max(k - std::abs(a - b), 0.0f) / k;
    return std::min(a, b) - h * h * k * 0.25f;
}

BakedMesh MeshBaker::marchingCubes(const std::vector<float>& densityField, int resolution)
{
    BakedMesh mesh;
    
    // Cube corner offsets (8 corners)
    const glm::ivec3 cornerOffsets[8] = {
        {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
        {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
    };
    
    // Edge connections (12 edges, each connects 2 corners)
    const int edgeConnections[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},  // Bottom face edges
        {4, 5}, {5, 6}, {6, 7}, {7, 4},  // Top face edges
        {0, 4}, {1, 5}, {2, 6}, {3, 7}   // Vertical edges
    };
    
    // Isosurface threshold
    const float isoLevel = 0.0f;
    
    // Grid spacing
    float step = 4.0f / (resolution - 1);
    float offset = -2.0f;
    
    // Vertex deduplication map
    struct EdgeKey {
        int x, y, z, edge;
        bool operator==(const EdgeKey& other) const {
            return x == other.x && y == other.y && z == other.z && edge == other.edge;
        }
    };
    
    struct EdgeKeyHash {
        size_t operator()(const EdgeKey& k) const {
            return ((size_t)k.x * 73856093) ^ ((size_t)k.y * 19349663) ^ 
                   ((size_t)k.z * 83492791) ^ ((size_t)k.edge * 1572869);
        }
    };
    
    std::unordered_map<EdgeKey, uint32_t, EdgeKeyHash> edgeToVertex;
    
    // Process each cube in the grid
    for (int z = 0; z < resolution - 1; ++z)
    {
        for (int y = 0; y < resolution - 1; ++y)
        {
            for (int x = 0; x < resolution - 1; ++x)
            {
                // Get density values at 8 cube corners
                float cornerValues[8];
                glm::vec3 cornerPositions[8];
                
                for (int i = 0; i < 8; ++i)
                {
                    int cx = x + cornerOffsets[i].x;
                    int cy = y + cornerOffsets[i].y;
                    int cz = z + cornerOffsets[i].z;
                    
                    cornerValues[i] = densityField[getFieldIndex(cx, cy, cz, resolution)];
                    cornerPositions[i] = glm::vec3(
                        offset + cx * step,
                        offset + cy * step,
                        offset + cz * step
                    );
                }
                
                // Determine cube configuration (8-bit index)
                int cubeIndex = 0;
                for (int i = 0; i < 8; ++i)
                {
                    if (cornerValues[i] > isoLevel)
                    {
                        cubeIndex |= (1 << i);
                    }
                }
                
                // Skip if cube is entirely inside or outside
                if (MarchingCubes::edgeTable[cubeIndex] == 0)
                {
                    continue;
                }
                
                // Find vertices on edges
                uint32_t vertexIndices[12];
                
                for (int i = 0; i < 12; ++i)
                {
                    if (MarchingCubes::edgeTable[cubeIndex] & (1 << i))
                    {
                        EdgeKey key = {x, y, z, i};
                        
                        // Check if vertex already exists
                        auto it = edgeToVertex.find(key);
                        if (it != edgeToVertex.end())
                        {
                            vertexIndices[i] = it->second;
                            continue;
                        }
                        
                        // Interpolate vertex position on edge
                        int c1 = edgeConnections[i][0];
                        int c2 = edgeConnections[i][1];
                        
                        float v1 = cornerValues[c1];
                        float v2 = cornerValues[c2];
                        
                        // Linear interpolation
                        float t = (isoLevel - v1) / (v2 - v1);
                        t = std::max(0.0f, std::min(1.0f, t));
                        
                        glm::vec3 p1 = cornerPositions[c1];
                        glm::vec3 p2 = cornerPositions[c2];
                        glm::vec3 vertexPos = p1 + t * (p2 - p1);
                        
                        // Calculate normals at both corners and interpolate
                        glm::vec3 normal1 = calculateGradient(densityField, 
                            x + cornerOffsets[c1].x, 
                            y + cornerOffsets[c1].y, 
                            z + cornerOffsets[c1].z, 
                            resolution);
                        
                        glm::vec3 normal2 = calculateGradient(densityField, 
                            x + cornerOffsets[c2].x, 
                            y + cornerOffsets[c2].y, 
                            z + cornerOffsets[c2].z, 
                            resolution);
                        
                        // Interpolate normals
                        glm::vec3 normal = glm::normalize(normal1 * (1.0f - t) + normal2 * t);
                        
                        // Add vertex
                        uint32_t vertexIndex = static_cast<uint32_t>(mesh.positions.size());
                        mesh.positions.push_back(vertexPos);
                        mesh.normals.push_back(normal);
                        
                        edgeToVertex[key] = vertexIndex;
                        vertexIndices[i] = vertexIndex;
                    }
                }
                
                // Generate triangles (reverse winding order for correct front-facing)
                for (int i = 0; MarchingCubes::triTable[cubeIndex][i] != -1; i += 3)
                {
                    mesh.indices.push_back(vertexIndices[MarchingCubes::triTable[cubeIndex][i]]);
                    mesh.indices.push_back(vertexIndices[MarchingCubes::triTable[cubeIndex][i + 2]]);
                    mesh.indices.push_back(vertexIndices[MarchingCubes::triTable[cubeIndex][i + 1]]);
                }
            }
        }
    }
    
    std::cout << "Marching cubes generated " << mesh.positions.size() << " vertices, " 
              << (mesh.indices.size() / 3) << " triangles\n";
    
    return mesh;
}

void MeshBaker::writeMeshToBinary(const BakedMesh& mesh, const std::string& filepath)
{
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file for writing: " + filepath);
    }
    
    try
    {
        // Write header
        uint32_t magic = 0x4853454D; // 'MESH'
        uint32_t version = 2; // Incremented version for curve data
        uint32_t vertexCount = static_cast<uint32_t>(mesh.positions.size());
        uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
        uint32_t curvePointCount = static_cast<uint32_t>(mesh.blendCurve.size());
        
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
        file.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
        file.write(reinterpret_cast<const char*>(&mesh.sizeRatio), sizeof(mesh.sizeRatio));
        file.write(reinterpret_cast<const char*>(&mesh.distanceRatio), sizeof(mesh.distanceRatio));
        file.write(reinterpret_cast<const char*>(&mesh.baseBlendingStrength), sizeof(mesh.baseBlendingStrength));
        file.write(reinterpret_cast<const char*>(&curvePointCount), sizeof(curvePointCount));
        
        // Write vertex positions
        file.write(reinterpret_cast<const char*>(mesh.positions.data()), 
                   mesh.positions.size() * sizeof(glm::vec3));
        
        // Write vertex normals
        file.write(reinterpret_cast<const char*>(mesh.normals.data()), 
                   mesh.normals.size() * sizeof(glm::vec3));
        
        // Write indices
        file.write(reinterpret_cast<const char*>(mesh.indices.data()), 
                   mesh.indices.size() * sizeof(uint32_t));
        
        // Write blend curve points
        for (const auto& point : mesh.blendCurve)
        {
            file.write(reinterpret_cast<const char*>(&point.distanceRatio), sizeof(point.distanceRatio));
            file.write(reinterpret_cast<const char*>(&point.blendMultiplier), sizeof(point.blendMultiplier));
        }
        
        file.close();
    }
    catch (const std::exception& e)
    {
        file.close();
        std::filesystem::remove(filepath); // Clean up partial file
        throw std::runtime_error("Error writing mesh data: " + std::string(e.what()));
    }
}

BakedMesh MeshBaker::readMeshFromBinary(const std::string& filepath)
{
    BakedMesh mesh;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file for reading: " + filepath);
    }
    
    try
    {
        // Read header
        uint32_t magic, version, vertexCount, indexCount, curvePointCount;
        
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        file.read(reinterpret_cast<char*>(&mesh.sizeRatio), sizeof(mesh.sizeRatio));
        file.read(reinterpret_cast<char*>(&mesh.distanceRatio), sizeof(mesh.distanceRatio));
        file.read(reinterpret_cast<char*>(&mesh.baseBlendingStrength), sizeof(mesh.baseBlendingStrength));
        file.read(reinterpret_cast<char*>(&curvePointCount), sizeof(curvePointCount));
        
        // Validate magic number
        if (magic != 0x4853454D) // 'MESH'
        {
            throw std::runtime_error("Invalid mesh file format (bad magic number)");
        }
        
        // Validate version
        if (version != 2)
        {
            throw std::runtime_error("Unsupported mesh file version: " + std::to_string(version));
        }
        
        // Validate counts
        if (vertexCount == 0 || indexCount == 0 || (indexCount % 3) != 0)
        {
            throw std::runtime_error("Invalid mesh data counts");
        }
        
        // Read vertex positions
        mesh.positions.resize(vertexCount);
        file.read(reinterpret_cast<char*>(mesh.positions.data()), 
                  vertexCount * sizeof(glm::vec3));
        
        // Read vertex normals
        mesh.normals.resize(vertexCount);
        file.read(reinterpret_cast<char*>(mesh.normals.data()), 
                  vertexCount * sizeof(glm::vec3));
        
        // Read indices
        mesh.indices.resize(indexCount);
        file.read(reinterpret_cast<char*>(mesh.indices.data()), 
                  indexCount * sizeof(uint32_t));
        
        // Read blend curve points
        mesh.blendCurve.resize(curvePointCount);
        for (uint32_t i = 0; i < curvePointCount; ++i)
        {
            file.read(reinterpret_cast<char*>(&mesh.blendCurve[i].distanceRatio), 
                     sizeof(mesh.blendCurve[i].distanceRatio));
            file.read(reinterpret_cast<char*>(&mesh.blendCurve[i].blendMultiplier), 
                     sizeof(mesh.blendCurve[i].blendMultiplier));
        }
        
        file.close();
        
        // Final validation
        if (!mesh.isValid())
        {
            throw std::runtime_error("Loaded mesh failed validation");
        }
        
        std::cout << "Loaded mesh from " << filepath << ": " 
                  << mesh.positions.size() << " vertices, " 
                  << (mesh.indices.size() / 3) << " triangles\n";
    }
    catch (const std::exception& e)
    {
        file.close();
        throw std::runtime_error("Error reading mesh data: " + std::string(e.what()));
    }
    
    return mesh;
}

bool MeshBaker::validateMeshFile(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open())
    {
        return false;
    }
    
    try
    {
        // Read and validate header
        uint32_t magic, version, vertexCount, indexCount, curvePointCount;
        
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
        file.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
        
        // Skip metadata
        file.seekg(sizeof(float) * 3, std::ios::cur); // sizeRatio, distanceRatio, baseBlendingStrength
        
        file.read(reinterpret_cast<char*>(&curvePointCount), sizeof(curvePointCount));
        
        // Validate magic number
        if (magic != 0x4853454D) // 'MESH'
        {
            file.close();
            return false;
        }
        
        // Validate version
        if (version != 2)
        {
            file.close();
            return false;
        }
        
        // Validate counts
        if (vertexCount == 0 || indexCount == 0 || (indexCount % 3) != 0)
        {
            file.close();
            return false;
        }
        
        // Calculate expected file size
        size_t expectedSize = sizeof(uint32_t) * 4 + // magic, version, vertexCount, indexCount
                             sizeof(float) * 3 +      // sizeRatio, distanceRatio, baseBlendingStrength
                             sizeof(uint32_t) +       // curvePointCount
                             vertexCount * sizeof(glm::vec3) * 2 + // positions + normals
                             indexCount * sizeof(uint32_t) +       // indices
                             curvePointCount * sizeof(float) * 2;  // curve points
        
        // Get actual file size
        file.seekg(0, std::ios::end);
        size_t actualSize = file.tellg();
        
        file.close();
        
        // Validate file size matches expected
        return actualSize == expectedSize;
    }
    catch (const std::exception&)
    {
        file.close();
        return false;
    }
}

std::string MeshBaker::generateFilename(float sizeRatio, float distanceRatio)
{
    return "bridge_r" + std::to_string(sizeRatio) + "_d" + std::to_string(distanceRatio) + ".mesh";
}

glm::vec3 MeshBaker::calculateGradient(const std::vector<float>& field, int x, int y, int z, int resolution)
{
    // Central difference gradient calculation
    glm::vec3 gradient(0.0f);
    
    if (x > 0 && x < resolution - 1)
    {
        gradient.x = field[getFieldIndex(x + 1, y, z, resolution)] - 
                     field[getFieldIndex(x - 1, y, z, resolution)];
    }
    
    if (y > 0 && y < resolution - 1)
    {
        gradient.y = field[getFieldIndex(x, y + 1, z, resolution)] - 
                     field[getFieldIndex(x, y - 1, z, resolution)];
    }
    
    if (z > 0 && z < resolution - 1)
    {
        gradient.z = field[getFieldIndex(x, y, z + 1, resolution)] - 
                     field[getFieldIndex(x, y, z - 1, resolution)];
    }
    
    // Normalize gradient to get normal
    // Negate because gradient points toward higher density (inward)
    // We want normals pointing outward from the surface
    float len = glm::length(gradient);
    if (len > 0.0001f)
    {
        return -gradient / len;
    }
    return glm::vec3(0.0f, 1.0f, 0.0f); // Fallback normal
}

int MeshBaker::getFieldIndex(int x, int y, int z, int resolution)
{
    return x + y * resolution + z * resolution * resolution;
}

float MeshBaker::catmullRomInterpolate(float t, float p0, float p1, float p2, float p3) const
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                  (-p0 + p2) * t +
                  (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                  (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

float MeshBaker::interpolateBlendMultiplier(float distanceRatio, const std::vector<BlendCurvePoint>& curve) const
{
    if (curve.empty())
    {
        return 1.0f;
    }
    
    if (curve.size() == 1)
    {
        return curve[0].blendMultiplier;
    }
    
    // Before first point
    if (distanceRatio <= curve.front().distanceRatio)
    {
        return curve.front().blendMultiplier;
    }
    
    // After last point
    if (distanceRatio >= curve.back().distanceRatio)
    {
        return curve.back().blendMultiplier;
    }
    
    // Find the segment containing distanceRatio
    for (size_t i = 0; i < curve.size() - 1; ++i)
    {
        if (distanceRatio >= curve[i].distanceRatio && distanceRatio <= curve[i + 1].distanceRatio)
        {
            float t = (distanceRatio - curve[i].distanceRatio) / 
                     (curve[i + 1].distanceRatio - curve[i].distanceRatio);
            
            // Get 4 control points for Catmull-Rom
            float p0 = (i > 0) ? curve[i - 1].blendMultiplier : curve[i].blendMultiplier;
            float p1 = curve[i].blendMultiplier;
            float p2 = curve[i + 1].blendMultiplier;
            float p3 = (i + 2 < curve.size()) ? curve[i + 2].blendMultiplier : curve[i + 1].blendMultiplier;
            
            return catmullRomInterpolate(t, p0, p1, p2, p3);
        }
    }
    
    return 1.0f;
}

void MeshBaker::writeCurveMetadata(const std::string& outputDirectory, float baseBlendingStrength,
                                  const std::vector<SizeRatioBlendCurve>& curves)
{
    std::string filepath = outputDirectory + "blend_curves.meta";
    std::ofstream file(filepath, std::ios::binary);
    
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open curve metadata file for writing: " + filepath);
    }
    
    try
    {
        // Write header
        uint32_t magic = 0x56525543; // 'CURV'
        uint32_t version = 2;  // Version 2 for multi-curve support
        uint32_t numSizeRatios = static_cast<uint32_t>(curves.size());
        
        file.write(reinterpret_cast<const char*>(&magic), sizeof(magic));
        file.write(reinterpret_cast<const char*>(&version), sizeof(version));
        file.write(reinterpret_cast<const char*>(&baseBlendingStrength), sizeof(baseBlendingStrength));
        file.write(reinterpret_cast<const char*>(&numSizeRatios), sizeof(numSizeRatios));
        
        // Write each size ratio's curve
        for (const auto& curve : curves)
        {
            file.write(reinterpret_cast<const char*>(&curve.sizeRatio), sizeof(curve.sizeRatio));
            uint32_t numPoints = static_cast<uint32_t>(curve.curvePoints.size());
            file.write(reinterpret_cast<const char*>(&numPoints), sizeof(numPoints));
            
            for (const auto& point : curve.curvePoints)
            {
                file.write(reinterpret_cast<const char*>(&point.distanceRatio), sizeof(point.distanceRatio));
                file.write(reinterpret_cast<const char*>(&point.blendMultiplier), sizeof(point.blendMultiplier));
            }
        }
        
        file.close();
        std::cout << "Wrote blend curve metadata with " << curves.size() << " size ratio curves: " << filepath << "\n";
    }
    catch (const std::exception& e)
    {
        file.close();
        std::filesystem::remove(filepath);
        throw std::runtime_error("Error writing curve metadata: " + std::string(e.what()));
    }
}

std::vector<SizeRatioBlendCurve> MeshBaker::loadCurveMetadata(const std::string& filepath, float& outBaseBlendingStrength)
{
    std::vector<SizeRatioBlendCurve> curves;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open curve metadata file for reading: " + filepath);
    }
    
    try
    {
        // Read header
        uint32_t magic, version;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&outBaseBlendingStrength), sizeof(outBaseBlendingStrength));
        
        // Validate magic number
        if (magic != 0x56525543)
        {
            throw std::runtime_error("Invalid curve metadata file format");
        }
        
        if (version == 2)
        {
            // New multi-curve format
            uint32_t numSizeRatios;
            file.read(reinterpret_cast<char*>(&numSizeRatios), sizeof(numSizeRatios));
            
            curves.reserve(numSizeRatios);
            for (uint32_t i = 0; i < numSizeRatios; ++i)
            {
                SizeRatioBlendCurve curve;
                file.read(reinterpret_cast<char*>(&curve.sizeRatio), sizeof(curve.sizeRatio));
                
                uint32_t numPoints;
                file.read(reinterpret_cast<char*>(&numPoints), sizeof(numPoints));
                
                curve.curvePoints.reserve(numPoints);
                for (uint32_t j = 0; j < numPoints; ++j)
                {
                    BlendCurvePoint point;
                    file.read(reinterpret_cast<char*>(&point.distanceRatio), sizeof(point.distanceRatio));
                    file.read(reinterpret_cast<char*>(&point.blendMultiplier), sizeof(point.blendMultiplier));
                    curve.curvePoints.push_back(point);
                }
                
                curves.push_back(curve);
            }
            
            std::cout << "Loaded blend curve metadata: " << numSizeRatios << " size ratio curves\n";
        }
        else if (version == 1)
        {
            // Legacy single-curve format - convert to multi-curve
            uint32_t numPoints;
            file.read(reinterpret_cast<char*>(&numPoints), sizeof(numPoints));
            
            std::vector<BlendCurvePoint> legacyCurve;
            legacyCurve.reserve(numPoints);
            for (uint32_t i = 0; i < numPoints; ++i)
            {
                BlendCurvePoint point;
                file.read(reinterpret_cast<char*>(&point.distanceRatio), sizeof(point.distanceRatio));
                file.read(reinterpret_cast<char*>(&point.blendMultiplier), sizeof(point.blendMultiplier));
                legacyCurve.push_back(point);
            }
            
            // Create a single curve with size ratio 1.0
            curves.push_back(SizeRatioBlendCurve(1.0f, legacyCurve));
            
            std::cout << "Loaded legacy blend curve metadata (converted to multi-curve format)\n";
        }
        else
        {
            throw std::runtime_error("Unsupported curve metadata version: " + std::to_string(version));
        }
        
        file.close();
    }
    catch (const std::exception& e)
    {
        file.close();
        throw std::runtime_error("Error reading curve metadata: " + std::string(e.what()));
    }
    
    return curves;
}

std::vector<BlendCurvePoint> MeshBaker::getCurveForSizeRatio(float sizeRatio, 
                                                             const std::vector<SizeRatioBlendCurve>& curves) const
{
    if (curves.empty())
    {
        // Return default curve
        return {{0.1f, 0.5f}, {1.0f, 1.0f}, {2.0f, 1.5f}, {3.0f, 0.2f}};
    }
    
    // Find exact match
    for (const auto& curve : curves)
    {
        if (std::abs(curve.sizeRatio - sizeRatio) < 0.001f)
        {
            return curve.curvePoints;
        }
    }
    
    // Find surrounding curves for interpolation
    const SizeRatioBlendCurve* lowerCurve = nullptr;
    const SizeRatioBlendCurve* upperCurve = nullptr;
    float t = 0.0f;
    
    // Before first curve
    if (sizeRatio <= curves.front().sizeRatio)
    {
        return curves.front().curvePoints;
    }
    
    // After last curve
    if (sizeRatio >= curves.back().sizeRatio)
    {
        return curves.back().curvePoints;
    }
    
    // Find surrounding curves
    for (size_t i = 0; i < curves.size() - 1; ++i)
    {
        if (sizeRatio >= curves[i].sizeRatio && sizeRatio <= curves[i + 1].sizeRatio)
        {
            lowerCurve = &curves[i];
            upperCurve = &curves[i + 1];
            t = (sizeRatio - curves[i].sizeRatio) / (curves[i + 1].sizeRatio - curves[i].sizeRatio);
            break;
        }
    }
    
    if (!lowerCurve || !upperCurve)
    {
        return curves.front().curvePoints;
    }
    
    // Interpolate curves - create a new curve with interpolated control points
    // Assume both curves have the same distance ratio points (they should from UI)
    std::vector<BlendCurvePoint> interpolatedCurve;
    
    size_t minSize = std::min(lowerCurve->curvePoints.size(), upperCurve->curvePoints.size());
    for (size_t i = 0; i < minSize; ++i)
    {
        BlendCurvePoint point;
        point.distanceRatio = lowerCurve->curvePoints[i].distanceRatio * (1.0f - t) + 
                             upperCurve->curvePoints[i].distanceRatio * t;
        point.blendMultiplier = lowerCurve->curvePoints[i].blendMultiplier * (1.0f - t) + 
                               upperCurve->curvePoints[i].blendMultiplier * t;
        interpolatedCurve.push_back(point);
    }
    
    return interpolatedCurve;
}

float MeshBaker::getRuntimeBlendMultiplier(float sizeRatio, float distanceRatio, 
                                          const std::vector<SizeRatioBlendCurve>& curves) const
{
    if (curves.empty())
    {
        return 1.0f;
    }
    
    // Get interpolated curve for this size ratio
    std::vector<BlendCurvePoint> curve = getCurveForSizeRatio(sizeRatio, curves);
    
    // Evaluate blend multiplier on the curve
    return interpolateBlendMultiplier(distanceRatio, curve);
}

const SizeRatioBlendCurve* MeshBaker::findCurveForSizeRatio(float sizeRatio, 
                                                            const std::vector<SizeRatioBlendCurve>& curves) const
{
    for (const auto& curve : curves)
    {
        if (std::abs(curve.sizeRatio - sizeRatio) < 0.001f)
        {
            return &curve;
        }
    }
    return nullptr;
}
