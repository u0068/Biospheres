#version 450 core

layout (location = 0) in vec3 aPos;    // Ring vertex position
layout (location = 1) in vec3 aNormal; // Ring vertex normal (for top/bottom coloring)

uniform mat4 uModel;      // Model matrix (per-cell transformation)
uniform mat4 uView;       // View matrix (camera)
uniform mat4 uProjection; // Projection matrix

out vec3 worldPos;        // World space position for consistent coloring
out vec3 localPos;        // Local position relative to ring center

void main()
{
    // Transform vertex position
    vec4 worldPosition = uModel * vec4(aPos, 1.0);
    worldPos = worldPosition.xyz;
    localPos = aPos; // Store original local position
    
    gl_Position = uProjection * uView * worldPosition;
}
