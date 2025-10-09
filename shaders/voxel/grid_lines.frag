#version 460 core

out vec4 FragColor;

uniform vec3 u_lineColor;
uniform float u_lineAlpha;

void main()
{
    FragColor = vec4(u_lineColor, u_lineAlpha);
}
