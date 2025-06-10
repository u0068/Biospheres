#version 430 core

struct Cell 
{
    // Match the ComputeCell structure from compute shaders
    vec4 positionAndRadius;  // x, y, z, radius
    vec4 velocityAndMass;    // vx, vy, vz, mass
    vec4 acceleration;       // ax, ay, az, unused
};

layout(std430, binding = 0) buffer CellBuffer {
    Cell cells[];
};

out vec4 fragColor;

uniform int u_cellCount;
uniform vec2 u_resolution;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraFront;
uniform vec3 u_cameraRight;
uniform vec3 u_cameraUp;

// Screen-space sphere intersection
bool intersectSphere(vec3 rayOrigin, vec3 rayDir, vec3 sphereCenter, float sphereRadius, out float t) {
    vec3 oc = rayOrigin - sphereCenter;
    float a = dot(rayDir, rayDir);
    float b = 2.0 * dot(oc, rayDir);
    float c = dot(oc, oc) - sphereRadius * sphereRadius;
    float discriminant = b * b - 4.0 * a * c;
    
    if (discriminant < 0.0) {
        return false;
    }
    
    float sqrt_discriminant = sqrt(discriminant);
    float t1 = (-b - sqrt_discriminant) / (2.0 * a);
    float t2 = (-b + sqrt_discriminant) / (2.0 * a);
    
    // Use the closest positive intersection
    if (t1 > 0.0) {
        t = t1;
        return true;
    } else if (t2 > 0.0) {
        t = t2;
        return true;
    }
    
    return false;
}

vec3 getRayDirection(vec2 uv) {
    // Convert screen coordinates to normalized device coordinates
    vec2 ndc = (uv / u_resolution) * 2.0 - 1.0;
    
    // Apply aspect ratio correction
    float aspectRatio = u_resolution.x / u_resolution.y;
    ndc.x *= aspectRatio;
    
    // Field of view (in radians)
    float fovRadians = radians(45.0);
    float tanHalfFov = tan(fovRadians * 0.5);
    
    // Scale by FOV
    ndc *= tanHalfFov;
    
    // Create ray direction using camera basis vectors
    vec3 rayDir = normalize(u_cameraFront + u_cameraRight * ndc.x + u_cameraUp * ndc.y);
    return rayDir;
}

// Efficient screen-space sphere rendering
void main() {
    vec2 uv = gl_FragCoord.xy;
    vec3 rayOrigin = u_cameraPos;
    vec3 rayDir = getRayDirection(uv);
    
    float closestT = 999999.0;
    int closestCell = -1;
    
    // Find the closest sphere intersection
    for (int i = 0; i < u_cellCount; ++i) {
        vec3 sphereCenter = cells[i].positionAndRadius.xyz;
        float sphereRadius = cells[i].positionAndRadius.w;
        
        float t;
        if (intersectSphere(rayOrigin, rayDir, sphereCenter, sphereRadius, t)) {
            if (t < closestT) {
                closestT = t;
                closestCell = i;
            }
        }
    }
    
    if (closestCell >= 0) {
        // Calculate intersection point and normal
        vec3 hitPoint = rayOrigin + rayDir * closestT;
        vec3 sphereCenter = cells[closestCell].positionAndRadius.xyz;
        vec3 normal = normalize(hitPoint - sphereCenter);
        
        // Simple lighting calculation
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
        float lighting = max(0.1, dot(normal, lightDir));
        
        // Color based on cell index for variety
        vec3 baseColor = vec3(
            0.5 + 0.5 * sin(float(closestCell) * 0.1),
            0.5 + 0.5 * sin(float(closestCell) * 0.1 + 2.0),
            0.5 + 0.5 * sin(float(closestCell) * 0.1 + 4.0)
        );
        
        fragColor = vec4(baseColor * lighting, 1.0);
    } else {
        // Background color
        fragColor = vec4(0.1, 0.1, 0.2, 1.0);
    }
}
