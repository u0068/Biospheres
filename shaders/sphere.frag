#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vCameraPos;
in float vRadius;
in vec3 vInstanceCenter;

out vec4 fragColor;

uniform vec3 uLightDir = vec3(1.0, 1.0, 1.0);
uniform vec3 uSelectedCellPos = vec3(-9999.0); // Position of selected cell, invalid position by default
uniform float uSelectedCellRadius = 0.0;
uniform float uTime = 0.0; // For animation effects

void main() {
    // Calculate distance from camera for depth-based effects
    float distanceToCamera = length(vWorldPos - vCameraPos);
    
    // Check if this is the selected cell
    float distanceToSelected = length(vInstanceCenter - uSelectedCellPos);
    bool isSelected = distanceToSelected < 0.1 && uSelectedCellRadius > 0.0;
    
    // Normalize the normal (interpolation might have changed its length)
    vec3 normal = normalize(vNormal);
    
    // Simple lighting calculation
    vec3 lightDir = normalize(uLightDir);
    float lambertian = max(0.1, dot(normal, lightDir));
      // Add some specular reflection
    vec3 viewDir = normalize(vCameraPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(0.0, dot(viewDir, reflectDir)), 32.0) * 0.3;
    
    // Base color is white by default
    vec3 baseColor = vec3(0.9, 0.9, 0.9); // Slightly off-white for better lighting visibility
    
    // Highlight selected cell
    if (isSelected) {
        float pulse = 0.5 + 0.5 * sin(uTime * 6.0); // Pulsing effect
        baseColor = mix(baseColor, vec3(1.0, 1.0, 0.0), 0.3 + 0.2 * pulse); // Yellow highlight with pulse
        specular *= 2.0; // Increase specular for selected cell
    }
    
    // Combine lighting
    vec3 finalColor = baseColor * lambertian + vec3(1.0) * specular;
    
    // Add some depth fog for distant spheres
    float fogFactor = 1.0 / (1.0 + distanceToCamera * 0.01);
    finalColor = mix(vec3(0.1, 0.1, 0.2), finalColor, fogFactor);
    
    fragColor = vec4(finalColor, 1.0);
}
