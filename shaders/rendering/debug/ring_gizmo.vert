#version 430 core

layout(location = 0) in vec4 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vColor;

void main() {
    vColor = aColor.rgb;
    gl_Position = uProjection * uView * aPosition;
} 