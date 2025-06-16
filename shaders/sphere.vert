#version 430 core

struct InstanceData {
    vec4 positionAndRadius;
    vec4 color;
};

layout(location = 0) in vec3 aPosition;     // Sphere mesh vertex position
layout(location = 1) in vec3 aNormal;       // Sphere mesh vertex normal
layout(location = 2) in InstanceData aInstanceData;

uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uCameraPos;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vCameraPos;
out float vRadius;
out vec3 vInstanceCenter;
out vec3 vBaseColor;

void main() {
    // Scale the sphere vertex by the instance radius and translate by instance position
    vec3 worldPos = aInstanceData.positionAndRadius.xyz + (aPosition * aInstanceData.positionAndRadius.w);
    
    // Transform normal (no scaling needed for uniform scaling)
    vNormal = aNormal;
    vWorldPos = worldPos;
    vCameraPos = uCameraPos;
    vRadius = aInstanceData.positionAndRadius.w;
    vInstanceCenter = aInstanceData.positionAndRadius.xyz;
    vBaseColor = aInstanceData.color.rgb;

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
