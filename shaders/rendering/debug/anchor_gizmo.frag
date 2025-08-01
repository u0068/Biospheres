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

void main() {
    // DEBUG: Just output solid red color to ensure rendering works
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
    
    // TODO: Add proper lighting back once basic rendering is confirmed
    /*
    // Normalize the normal (interpolation might have changed its length)
    vec3 normal = normalize(vNormal);
    
    // Simple lighting calculation
    vec3 lightDir = normalize(uLightDir);
    vec3 viewDir = normalize(vCameraPos - vWorldPos);
    
    float lambertian = max(0.1, dot(normal, lightDir));
    
    // Add some specular reflection
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(0.0, dot(viewDir, reflectDir)), 32.0) * 0.3;
    
    // Use the base color (red for anchors)
    vec3 baseColor = vBaseColor;
    
    // Apply lighting
    vec3 finalColor = baseColor * lambertian + vec3(specular);
    
    fragColor = vec4(finalColor, 1.0);
    */
} 