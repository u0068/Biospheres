#version 460 core

// Simple fragment shader for flow line rendering

// Input from vertex shader
in VS_OUT {
    float velocityMagnitude;
} fs_in;

// Uniforms
uniform vec4 u_baseLineColor;
uniform int u_enableFlowLines;

// Output
out vec4 fragColor;

void main() {
    if (u_enableFlowLines == 0) {
        discard;
    }
    
    // Skip if velocity is zero
    if (fs_in.velocityMagnitude <= 0.0) {
        discard;
    }
    
    // Use bright magenta color for maximum visibility
    fragColor = u_baseLineColor;
}