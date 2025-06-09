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

float sdSphere(vec3 p, vec3 center, float radius) {
    return length(p - center) - radius;
}

float sceneSDF(vec3 p) {
    float d = 9999.0;
    // Iterate through all cells to find the minimum distance to any cell
    for (int i = 0; i < u_cellCount; ++i) {
        vec3 center = cells[i].positionAndRadius.xyz;
        float r = cells[i].positionAndRadius.w;
        d = min(d, sdSphere(p, center, r));
    }
    return d;
}

vec3 getRayDirection(vec2 uv) {
    // Adjust this for your camera setup
    float fov = 1.0;
    vec2 ndc = (uv / u_resolution.x) * 2.0 - 1.0;
    return normalize(vec3(ndc.x, ndc.y, -fov));
}

// This runs for every pixel
void main() {
    vec2 uv = gl_FragCoord.xy;
    vec3 camPos = vec3(0.0, 0.0, 2.0);
    vec3 rayDir = getRayDirection(uv);

    float t = 0.0;
    float dist;
    for (int i = 0; i < 100; ++i) {
        vec3 p = camPos + rayDir * t;
        dist = sceneSDF(p);
        if (dist < 0.001) {
            fragColor = vec4(1.0, 0.5, 0.5, 1.0);
            return;
        }
        t += dist;
        if (t > 10.0) break;
    }

    fragColor = vec4(0.0);
}
