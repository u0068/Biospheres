#version 460 core

uniform vec4 u_brushColor;

out vec4 fragColor;

void main() {
    // Simple translucent brush rendering
    fragColor = u_brushColor;
}