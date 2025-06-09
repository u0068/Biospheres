#version 430 core
layout (location = 0) in vec3 aPos;
//uniform float size;
void main()
{
   //gl_Position = vec4(size * aPos.x, size * aPos.y, size * aPos.z, 1.0);
   gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);
}