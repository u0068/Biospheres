#version 430 core

layout(location = 0) in vec3 aPosition;         // Sphere mesh vertex
layout(location = 1) in vec3 aInstancePos;      // Cell position

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vPosition;

void main()
{
    vec3 worldPos = aInstancePos + aPosition; // No scaling yet
    vPosition = worldPos;
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
