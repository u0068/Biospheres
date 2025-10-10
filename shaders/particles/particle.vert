#version 460 core

// Vertex attributes (for a quad)
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec2 aTexCoord;

// Instance data
layout(location = 2) in vec3 iPosition;
layout(location = 3) in float iSize;
layout(location = 4) in vec4 iColor;
layout(location = 5) in float iLifetime;
layout(location = 6) in float iMaxLifetime;
layout(location = 7) in float iFadeFactor;

// Outputs
out vec2 TexCoord;
out vec4 ParticleColor;
out float Lifetime;
out float MaxLifetime;
out float FadeFactor;

// Uniforms
uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uCameraPos;

void main() {
    // Billboard the particle to face the camera
    vec3 cameraRight = vec3(uView[0][0], uView[1][0], uView[2][0]);
    vec3 cameraUp = vec3(uView[0][1], uView[1][1], uView[2][1]);
    
    // Create billboard quad
    vec3 worldPos = iPosition + (cameraRight * aPos.x + cameraUp * aPos.y) * iSize;
    
    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
    TexCoord = aTexCoord;
    ParticleColor = iColor;
    Lifetime = iLifetime;
    MaxLifetime = iMaxLifetime;
    FadeFactor = iFadeFactor;
}
