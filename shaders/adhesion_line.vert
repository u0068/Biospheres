#version 430 core
layout(location = 0) in vec3 inPosition;

uniform mat4 uViewProjection;

void main() {
    gl_Position = uViewProjection * vec4(inPosition, 1.0);
}
