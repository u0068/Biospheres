#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in float vFadeFactor;

out vec4 fragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir = vec3(1.0, 1.0, 1.0);
uniform vec3 uFogColor = vec3(0.1, 0.15, 0.3);

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    
    // Match phagocyte lighting
    float lambertian = max(0.1, dot(normal, lightDir));
    
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(0.0, dot(viewDir, reflectDir)), 32.0) * 0.3;
    
    vec3 finalColor = vColor * lambertian + vec3(1.0) * specular;
    
    // Apply distance-based fade factor
    finalColor *= vFadeFactor;
    
    // Add atmospheric scattering
    float atmosphericScatter = 1.0 - vFadeFactor;
    finalColor = mix(finalColor, uFogColor, atmosphericScatter * 0.5);
    
    fragColor = vec4(finalColor, 1.0);
}
