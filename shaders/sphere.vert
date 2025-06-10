#version 430 core

layout(location = 0) in vec3 aPosition;     // Sphere mesh vertex position
layout(location = 1) in vec3 aNormal;       // Sphere mesh vertex normal
layout(location = 2) in vec4 aInstanceData; // Instance data: xyz = position, w = radius

uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uCameraPos;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vCameraPos;
out float vRadius;
out vec3 vInstanceCenter;

void main() {
    // Scale the sphere vertex by the instance radius and translate by instance position
    vec3 worldPos = aInstanceData.xyz + (aPosition * aInstanceData.w);
    
    // Transform normal (no scaling needed for uniform scaling)
    vNormal = aNormal;
    vWorldPos = worldPos;
    vCameraPos = uCameraPos;
    vRadius = aInstanceData.w;
    vInstanceCenter = aInstanceData.xyz;
    
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
