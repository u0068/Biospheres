#include "blend_curve_loader.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

std::vector<BlendCurvePoint> BlendCurveLoader::loadCurveMetadata(const std::string& filepath,
                                                                 float& outBaseBlendingStrength)
{
    std::vector<BlendCurvePoint> curve;
    std::ifstream file(filepath, std::ios::binary);
    
    if (!file.is_open())
    {
        std::cerr << "Warning: Could not open curve metadata file: " << filepath << "\n";
        std::cerr << "Using default curve instead.\n";
        outBaseBlendingStrength = 0.5f;
        return getDefaultCurve();
    }
    
    try
    {
        // Read header
        uint32_t magic, version, numPoints;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        file.read(reinterpret_cast<char*>(&outBaseBlendingStrength), sizeof(outBaseBlendingStrength));
        file.read(reinterpret_cast<char*>(&numPoints), sizeof(numPoints));
        
        // Validate magic number
        if (magic != 0x56525543) // 'CURV'
        {
            throw std::runtime_error("Invalid curve metadata file format");
        }
        
        // Read curve points
        curve.reserve(numPoints);
        for (uint32_t i = 0; i < numPoints; ++i)
        {
            BlendCurvePoint point;
            file.read(reinterpret_cast<char*>(&point.distanceRatio), sizeof(point.distanceRatio));
            file.read(reinterpret_cast<char*>(&point.blendMultiplier), sizeof(point.blendMultiplier));
            curve.push_back(point);
        }
        
        file.close();
        std::cout << "Loaded blend curve metadata: " << numPoints << " control points, "
                  << "base strength: " << outBaseBlendingStrength << "\n";
    }
    catch (const std::exception& e)
    {
        file.close();
        std::cerr << "Error reading curve metadata: " << e.what() << "\n";
        std::cerr << "Using default curve instead.\n";
        outBaseBlendingStrength = 0.5f;
        return getDefaultCurve();
    }
    
    return curve;
}

float BlendCurveLoader::interpolateBlendMultiplier(float distanceRatio,
                                                   const std::vector<BlendCurvePoint>& curve)
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

std::vector<BlendCurvePoint> BlendCurveLoader::getDefaultCurve()
{
    return {
        {0.1f, 0.5f},   // Close: reduced blending
        {1.0f, 1.0f},   // Medium: normal blending
        {2.0f, 1.5f},   // Far: increased blending
        {3.0f, 0.2f}    // Very far: minimal blending
    };
}

float BlendCurveLoader::catmullRomInterpolate(float t, float p0, float p1, float p2, float p3)
{
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                  (-p0 + p2) * t +
                  (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                  (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}
