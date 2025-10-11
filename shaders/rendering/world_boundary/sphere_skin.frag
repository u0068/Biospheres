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

void main()
{
    // Ambient lighting
    float ambient = 0.2;
    
    // Diffuse lighting
    vec3 norm = normalize(Normal);
    float diffuse = max(dot(norm, normalize(uLightDir)), 0.0);
    
    // Specular lighting
    float specularStrength = 0.5;
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 reflectDir = reflect(-normalize(uLightDir), norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    float specular = specularStrength * spec;
    
    // Combine lighting
    vec3 result = (ambient + diffuse + specular) * uSphereColor;
    
    // Add subtle pulsing effect
    float pulse = sin(uTime * 2.0) * 0.05 + 1.0;
    result *= pulse;
    
    // Apply transparency
    FragColor = vec4(result, uTransparency);
}
