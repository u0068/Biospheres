#version 430 core

// Structure of Arrays (SoA) approach for GPU-side cell data
layout(std430, binding = 0) buffer PositionBuffer {
    vec3 positions[];
};

layout(std430, binding = 1) buffer RadiusBuffer {
    float radii[];
};

out vec4 fragColor;

uniform int u_cellCount;
uniform vec2 u_resolution;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraFront;
uniform vec3 u_cameraRight;
uniform vec3 u_cameraUp;

const int MAX_ITERATIONS = 100;
const int MAX_DISTANCE = 100;
const float MIN_DISTANCE = 0.001;

float sphereSDF(vec3 point, vec3 center, float radius) {
    // Signed distance function for a sphere
    // Returns the distance from point p to the surface of the sphere
    // If the point is inside the sphere, it returns a negative value
    return length(point - center) - radius;
}

float sceneSDF(vec3 point) {
    float dist = 9999.0;
    // Iterate through all cells to find the minimum distance to any cell
    for (int i = 0; i < u_cellCount; ++i) {
        vec3 center = positions[i];
        float r = radii[i];
        dist = min(dist, sphereSDF(point, center, r)); // This can be changed to smooth min to make the cells blend together
    }
    return dist;
}

vec3 getNormal(vec3 point) {
    // Calculate the normal at a point by sampling the SDF in three directions
    float eps = 0.001;
    vec2 offset = vec2(eps, 0.0);
    vec3 n;
    n.x = sceneSDF(point + offset.xyy) - sceneSDF(point - offset.xyy);
    n.y = sceneSDF(point + offset.yxy) - sceneSDF(point - offset.yxy);
    n.z = sceneSDF(point + offset.yyx) - sceneSDF(point - offset.yyx);
    return normalize(n);
}

void main() {
    vec2 uv = (gl_FragCoord.xy / u_resolution.xy) * 2.0 - 1.0;
    uv.x *= u_resolution.x / u_resolution.y; // Correct aspect ratio
    
    // Create the ray for this pixel
    vec3 rayDir = normalize(u_cameraFront + uv.x * u_cameraRight + uv.y * u_cameraUp);
    vec3 rayPos = u_cameraPos;
    
    float totalDistance = 0.0;
    bool hit = false;
    
    // Raymarching loop
    for (int i = 0; i < MAX_ITERATIONS; ++i) {
        float dist = sceneSDF(rayPos);
        
        if (dist < MIN_DISTANCE) {
            hit = true;
            break;
        }
        
        if (totalDistance > MAX_DISTANCE) {
            break;
        }
        
        rayPos += rayDir * dist;
        totalDistance += dist;
    }
    
    if (hit) {
        // Calculate lighting
        vec3 normal = getNormal(rayPos);
        vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
        float diffuse = max(dot(normal, lightDir), 0.0);
        
        // Basic shading
        vec3 color = vec3(0.2, 0.6, 1.0) * (0.3 + 0.7 * diffuse);
        
        // Fog based on distance
        float fog = 1.0 - exp(-totalDistance * 0.02);
        color = mix(color, vec3(0.1, 0.1, 0.2), fog);
        
        fragColor = vec4(color, 1.0);
    } else {
        // Background color
        fragColor = vec4(0.05, 0.05, 0.1, 1.0);
    }
}
