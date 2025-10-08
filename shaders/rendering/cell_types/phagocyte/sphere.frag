#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vCameraPos;
in float vRadius;
in vec3 vInstanceCenter;
in vec3 vBaseColor;
in vec2 vUV;

out vec4 fragColor;

uniform vec3 uLightDir = vec3(1.0, 1.0, 1.0);
uniform vec3 uSelectedCellPos = vec3(-9999.0); // Position of selected cell, invalid position by default
uniform float uSelectedCellRadius = 0.0;
uniform float uTime = 0.0; // For animation effects
uniform sampler2D uTexture;

void main() {
    // Calculate distance from camera for depth-based effects
    float distanceToCamera = length(vWorldPos - vCameraPos);
    
    // Check if this is the selected cell
    float distanceToSelected = length(vInstanceCenter - uSelectedCellPos);
    bool isSelected = distanceToSelected < 0.1 && uSelectedCellRadius > 0.0;
    
    // Normalize the normal (interpolation might have changed its length)
    vec3 normal = normalize(vNormal);
    
    // DEBUG: Visualize normals as colors (comment out for normal rendering)
    // fragColor = vec4(normal * 0.5 + 0.5, 1.0);
    // return;
    
    // Simple lighting calculation
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(vCameraPos - vWorldPos);
    
    float lambertian = max(0.1, dot(normal, lightDir));
      // Add some specular reflection
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(0.0, dot(viewDir, reflectDir)), 32.0) * 0.3;
    
    // Base color is white by default
    vec3 baseColor = vBaseColor; // Slightly off-white for better lighting visibility
    
    // Sample texture using vUV
    vec3 texColor = texture(uTexture, vUV).rgb;
    baseColor *= texColor;
    
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
