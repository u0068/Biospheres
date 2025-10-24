#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>

/**
 * Blend curve control point for metaball blending
 */
struct BlendCurvePoint {
    float distanceRatio;
    float blendMultiplier;
};

/**
 * Loader for blend curve metadata used in metaball interpolation
 */
class BlendCurveLoader {
public:
    /**
     * Load blend curve from metadata file
     * @param filepath Path to blend_curve.meta file
     * @param outBaseBlendingStrength Output parameter for base blending strength
     * @return Vector of curve control points
     */
    static std::vector<BlendCurvePoint> loadCurveMetadata(const std::string& filepath, 
                                                          float& outBaseBlendingStrength);
    
    /**
     * Interpolate blend multiplier for a given distance ratio using Catmull-Rom spline
     * @param distanceRatio Distance ratio to evaluate
     * @param curve Curve control points
     * @return Interpolated blend multiplier
     */
    static float interpolateBlendMultiplier(float distanceRatio, 
                                           const std::vector<BlendCurvePoint>& curve);
    
    /**
     * Get default curve if metadata file doesn't exist
     * @return Default curve control points
     */
    static std::vector<BlendCurvePoint> getDefaultCurve();
    
private:
    static float catmullRomInterpolate(float t, float p0, float p1, float p2, float p3);
};
