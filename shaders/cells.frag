#version 430 core

struct Cell 
{
                            // Base Alignment	//Aligned Offset
    vec4 positionAndRadius; // 16 bytes         // 0 bytes      // x, y, z, radius
};

layout(std430, binding = 0) buffer CellBuffer {
    Cell cells[];
};

out vec4 fragColor;

uniform int u_cellCount;
uniform vec2 u_resolution;

const int MAX_ITERATIONS = 100;
const int MAX_DISTANCE = 100;
const float MIN_DISTANCE = 0.001;


float sphereSDF(vec3 p, vec3 center, float radius) {
    // Signed distance function for a sphere
    // Returns the distance from point p to the surface of the sphere
    // If the point is inside the sphere, it returns a negative value
    return length(p - center) - radius;
}

float sceneSDF(vec3 p) {
    float d = 9999.0;
    // Iterate through all cells to find the minimum distance to any cell
    for (int i = 0; i < u_cellCount; ++i) {
        vec3 center = cells[i].positionAndRadius.xyz;
        float r = cells[i].positionAndRadius.w;
        d = min(d, sphereSDF(p, center, r));
    }
    return d;
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

vec3 getRayDirection(vec2 uv) {
    // Adjust this for your camera setup
    float fov = 1.0;
    vec2 ndc = (uv / u_resolution.y) * 2.0 - 1.0;
    return normalize(vec3(ndc.x, ndc.y, -fov));
}

// This runs for every pixel
void main() {
    vec2 uv = gl_FragCoord.xy;
    vec3 camPos = vec3(0.0, 0.0, 2.0);
    vec3 rayDir = getRayDirection(uv);

    float t = 0.0; // This is the distance along the ray
    float dist; // Distance from the ray to the scene SDF
    float closestDist = 9999.0; // Initialize to a large value
    for (int i = 0; i < MAX_ITERATIONS; ++i) {
        vec3 p = camPos + rayDir * t;
        dist = sceneSDF(p);
        if (dist < MIN_DISTANCE) { // If the distance is very small, we are close to the surface
            vec3 normal = getNormal(p);
            fragColor = vec4(normal * 0.5 + 0.5, 1.0); // Color based on normal
            //fragColor = vec4(1.0, 0.5, 0.5, 1.0);
            return;
        } else if (dist < closestDist) {
            closestDist = dist; // Update the closest distance found
        }
        t += dist;
        if (t > MAX_DISTANCE) break;
    }

    fragColor = vec4(1/(10*closestDist + 1.)); // Fallback color if no surface is hit. For now, just a gradient based on closest approach to the surface
}
