#version 430 core

in vec3 vColor;
out vec4 fragColor;

void main() {
    // Output the color with full opacity
    fragColor = vec4(vColor, 1.0);
} 