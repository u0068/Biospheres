#version 460 core

in vec2 v_texCoord;
out vec4 fragColor;

// Camera uniforms
uniform mat4 u_invViewMatrix;
uniform mat4 u_invProjMatrix;
uniform vec3 u_cameraPos;
uniform vec2 u_resolution;

// Metaball parameters
uniform float u_sizeRatio;
uniform float u_distanceRatio;

// SDF blending parameters
uniform float u_blendingStrength;
uniform int u_numCurvePoints;
uniform float u_curveDistances[8];
uniform float u_curveMultipliers[8];

// Raymarching parameters
uniform int u_maxSteps;
uniform float u_maxDistance;
uniform float u_epsilon;

// SDF Functions
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}



float softMin(float a, float b, float k) {
    if (k <= 0.0) return min(a, b);
    
    // Smooth minimum using exponential blending
    // This creates smooth transitions without seams
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}



float evaluateDumbbellSDF(vec3 pos) {
    float smallRadius = 1.0;
    float largeRadius = smallRadius * u_sizeRatio;
    
    // Calculate distance from ratio
    float combinedRadius = smallRadius + largeRadius;
    float distance = u_distanceRatio * combinedRadius;
    
    // Position spheres
    vec3 leftCenter = vec3(-distance * 0.5, 0.0, 0.0);
    vec3 rightCenter = vec3(distance * 0.5, 0.0, 0.0);
    
    // Calculate SDF for each sphere (standard linear)
    float leftSphere = sdSphere(pos - leftCenter, smallRadius);
    float rightSphere = sdSphere(pos - rightCenter, largeRadius);
    
    // Catmull-Rom spline interpolation for smooth curves
    float blendMultiplier = 1.0;
    if (u_numCurvePoints > 0)
    {
        if (u_distanceRatio <= u_curveDistances[0])
        {
            blendMultiplier = u_curveMultipliers[0];
        }
        else if (u_distanceRatio >= u_curveDistances[u_numCurvePoints - 1])
        {
            blendMultiplier = u_curveMultipliers[u_numCurvePoints - 1];
        }
        else
        {
            for (int i = 0; i < u_numCurvePoints - 1; i++)
            {
                if (u_distanceRatio >= u_curveDistances[i] && u_distanceRatio <= u_curveDistances[i + 1])
                {
                    float t = (u_distanceRatio - u_curveDistances[i]) / (u_curveDistances[i + 1] - u_curveDistances[i]);
                    
                    // Get 4 control points for Catmull-Rom
                    float p0 = (i > 0) ? u_curveMultipliers[i - 1] : u_curveMultipliers[i];
                    float p1 = u_curveMultipliers[i];
                    float p2 = u_curveMultipliers[i + 1];
                    float p3 = (i + 2 < u_numCurvePoints) ? u_curveMultipliers[i + 2] : u_curveMultipliers[i + 1];
                    
                    // Catmull-Rom spline formula
                    float t2 = t * t;
                    float t3 = t2 * t;
                    blendMultiplier = 0.5 * ((2.0 * p1) +
                                            (-p0 + p2) * t +
                                            (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
                                            (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
                    break;
                }
            }
        }
    }
    
    float adaptiveBlendingStrength = u_blendingStrength * blendMultiplier;
    
    // Clamp the blending strength to prevent runaway expansion
    adaptiveBlendingStrength = clamp(adaptiveBlendingStrength, 0.1, 10.0);
    
    // Blend with soft minimum using distance-adaptive blending strength
    return softMin(leftSphere, rightSphere, adaptiveBlendingStrength);
}

// Calculate normal using gradient
vec3 calculateNormal(vec3 pos) {
    const float h = 0.001;
    return normalize(vec3(
        evaluateDumbbellSDF(pos + vec3(h, 0, 0)) - evaluateDumbbellSDF(pos - vec3(h, 0, 0)),
        evaluateDumbbellSDF(pos + vec3(0, h, 0)) - evaluateDumbbellSDF(pos - vec3(0, h, 0)),
        evaluateDumbbellSDF(pos + vec3(0, 0, h)) - evaluateDumbbellSDF(pos - vec3(0, 0, h))
    ));
}

// Raymarching
float raymarch(vec3 rayOrigin, vec3 rayDirection) {
    float totalDistance = 0.0;
    
    for (int i = 0; i < u_maxSteps; i++) {
        vec3 currentPos = rayOrigin + rayDirection * totalDistance;
        float distance = evaluateDumbbellSDF(currentPos);
        
        if (distance < u_epsilon) {
            return totalDistance; // Hit
        }
        
        totalDistance += distance;
        
        if (totalDistance > u_maxDistance) {
            break; // Miss
        }
    }
    
    return -1.0; // Miss
}

// Simple lighting
vec3 calculateLighting(vec3 pos, vec3 normal, vec3 viewDir) {
    // Light position
    vec3 lightPos = vec3(5.0, 5.0, 5.0);
    vec3 lightDir = normalize(lightPos - pos);
    
    // Ambient
    vec3 ambient = vec3(0.2, 0.2, 0.3);
    
    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * vec3(0.8, 0.6, 0.4);
    
    // Specular
    vec3 reflectDir = reflect(-lightDir, normal);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = spec * vec3(0.5, 0.5, 0.5);
    
    return ambient + diffuse + specular;
}

void main() {
    // Simple test to see if shader is working
    vec2 uv = v_texCoord;
    
    // Create a simple gradient to test if the shader is running
    vec3 testColor = vec3(uv.x, uv.y, 0.5);
    
    // Convert screen coordinates to NDC
    vec2 ndc = uv * 2.0 - 1.0;
    
    // Create ray in clip space
    vec4 clipPos = vec4(ndc, -1.0, 1.0);
    
    // Transform to view space
    vec4 viewPos = u_invProjMatrix * clipPos;
    viewPos = vec4(viewPos.xy, -1.0, 0.0);
    
    // Transform to world space
    vec3 worldDir = (u_invViewMatrix * viewPos).xyz;
    vec3 rayDirection = normalize(worldDir);
    vec3 rayOrigin = u_cameraPos;
    
    // Perform raymarching
    float t = raymarch(rayOrigin, rayDirection);
    
    if (t > 0.0) {
        // Hit - calculate shading
        vec3 hitPos = rayOrigin + rayDirection * t;
        vec3 normal = calculateNormal(hitPos);
        vec3 viewDir = normalize(u_cameraPos - hitPos);
        
        vec3 color = calculateLighting(hitPos, normal, viewDir);
        fragColor = vec4(color, 1.0);
    } else {
        // Miss - show test pattern to confirm shader is working
        fragColor = vec4(testColor, 1.0);
    }
}