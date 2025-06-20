#version 430 core

layout(location = 0) in vec3 aPosition;     // Line start/end position
layout(location = 1) in vec3 aColor;        // Line color (red, green, blue for forward, up, right)

uniform mat4 uProjection;
uniform mat4 uView;
uniform mat4 uModel;

out vec3 vColor;

void main() {
    vColor = aColor;
    gl_Position = uProjection * uView * uModel * vec4(aPosition, 1.0);
}
