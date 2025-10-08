#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vColor;
in vec3 vInstanceCenter;
in float vFadeFactor;

out vec4 fragColor;

uniform vec3 uCameraPos;
uniform vec3 uLightDir = vec3(1.0, 1.0, 1.0);
uniform vec3 uSelectedCellPos = vec3(-9999.0);
uniform float uSelectedCellRadius = 0.0;
uniform float uTime = 0.0;
uniform vec3 uFogColor = vec3(0.1, 0.15, 0.3);

void main() {
    // Check if this is the selected cell
    float distanceToSelected = length(vInstanceCenter - uSelectedCellPos);
    bool isSelected = distanceToSelected < 0.1 && uSelectedCellRadius > 0.0;
    
    // Normalize the normal
    vec3 normal = normalize(vNormal);
    
    // Simple lighting calculation (matching phagocyte)
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(uCameraPos - vWorldPos);
    
    float lambertian = max(0.1, dot(normal, lightDir));
    
    // Add specular reflection
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(0.0, dot(viewDir, reflectDir)), 32.0) * 0.3;
    
    // Base color
    vec3 baseColor = vColor;
    
    // Highlight selected cell
    if (isSelected) {
        float pulse = 0.5 + 0.5 * sin(uTime * 6.0);
        baseColor = mix(baseColor, vec3(1.0, 1.0, 0.0), 0.3 + 0.2 * pulse);
        specular *= 2.0;
    }
    
    // Combine lighting
    vec3 finalColor = baseColor * lambertian + vec3(1.0) * specular;
    
    // Apply distance-based fade factor
    finalColor *= vFadeFactor;
    
    // Add atmospheric scattering
    float atmosphericScatter = 1.0 - vFadeFactor;
    finalColor = mix(finalColor, uFogColor, atmosphericScatter * 0.5);
    
    fragColor = vec4(finalColor, 1.0);
}
