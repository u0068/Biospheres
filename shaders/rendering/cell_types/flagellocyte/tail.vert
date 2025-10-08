#version 430 core

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec4 aNormal;
layout(location = 2) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out float vFadeFactor;

void main() {
    vWorldPos = aPosition.xyz;
    vNormal = aNormal.xyz;
    vColor = aColor.rgb;
    vFadeFactor = aPosition.w; // Fade factor stored in position.w
    
    gl_Position = uProjection * uView * vec4(aPosition.xyz, 1.0);
}
