#version 460 core

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

out vec4 FragColor;

uniform vec3 uViewPos;
uniform vec3 uLightDir;
uniform vec3 uSphereColor;
uniform float uTransparency;
uniform float uTime;

// Distance fade uniforms
uniform float u_fadeStartDistance;
uniform float u_fadeEndDistance;

void main()
{
    // Calculate distance-based fade
    float distanceFromCamera = length(FragPos - uViewPos);
    float fadeFactor = 1.0;
    
    if (distanceFromCamera > u_fadeStartDistance) {
        fadeFactor = 1.0 - clamp((distanceFromCamera - u_fadeStartDistance) / (u_fadeEndDistance - u_fadeStartDistance), 0.0, 1.0);
    }
    
    // Simple but proper lighting
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(uLightDir);
    
    // Ambient lighting
    float ambient = 0.3;
    
    // Diffuse lighting
    float diffuse = max(dot(norm, lightDir), 0.0);
    
    // Combine lighting
    float lightFactor = ambient + diffuse * 0.7;
    
    vec3 result = lightFactor * uSphereColor;
    
    // Apply distance fade and transparency
    float finalAlpha = uTransparency * fadeFactor;
    FragColor = vec4(result, finalAlpha);
}
