#version 460 core

// Simple vertex shader for instanced flow line rendering

layout (location = 0) in vec3 a_position; // Line vertex position (0.0 or 1.0 for start/end)
layout (location = 1) in vec4 a_lineStart; // [startPos.xyz, length]
layout (location = 2) in vec4 a_lineData;  // [direction.xyz, magnitude]

// Uniforms
uniform mat4 u_modelMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform int u_enableFlowLines;

// Output to fragment shader
out VS_OUT {
    float velocityMagnitude;
} vs_out;

void main() {
    if (u_enableFlowLines == 0) {
        gl_Position = vec4(0.0);
        return;
    }
    
    // Skip if line has zero length
    if (a_lineStart.w <= 0.0) {
        gl_Position = vec4(0.0);
        return;
    }
    
    // Calculate world position along the line
    vec3 startPos = a_lineStart.xyz;
    vec3 direction = a_lineData.xyz;
    float length = a_lineStart.w;
    
    // Position along line (a_position.x is 0.0 or 1.0)
    vec3 worldPos = startPos + direction * length * a_position.x;
    
    // Transform to clip space
    vec4 viewPos = u_viewMatrix * u_modelMatrix * vec4(worldPos, 1.0);
    gl_Position = u_projectionMatrix * viewPos;
    
    // Pass velocity magnitude to fragment shader
    vs_out.velocityMagnitude = a_lineData.w;
}