#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vCameraPos;
in float vRadius;
in vec3 vInstanceCenter;

out vec4 fragColor;

uniform vec3 uLightDir = vec3(1.0, 1.0, 1.0);

void main() {
    // Calculate distance from camera for depth-based effects
    float distanceToCamera = length(vWorldPos - vCameraPos);
    
    // Normalize the normal (interpolation might have changed its length)
    vec3 normal = normalize(vNormal);
    
    // Simple lighting calculation
    vec3 lightDir = normalize(uLightDir);
    float lambertian = max(0.1, dot(normal, lightDir));
    
    // Add some specular reflection
    vec3 viewDir = normalize(vCameraPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(0.0, dot(viewDir, reflectDir)), 32.0) * 0.3;
    
    // Color based on position for variety (you can change this to use instance ID later)
    float hash = sin(vInstanceCenter.x * 0.1) * sin(vInstanceCenter.y * 0.1) * sin(vInstanceCenter.z * 0.1);
    vec3 baseColor = vec3(
        0.5 + 0.5 * sin(hash),
        0.5 + 0.5 * sin(hash + 2.0),
        0.5 + 0.5 * sin(hash + 4.0)
    );
    
    // Combine lighting
    vec3 finalColor = baseColor * lambertian + vec3(1.0) * specular;
    
    // Add some depth fog for distant spheres
    float fogFactor = 1.0 / (1.0 + distanceToCamera * 0.01);
    finalColor = mix(vec3(0.1, 0.1, 0.2), finalColor, fogFactor);
    
    fragColor = vec4(finalColor, 1.0);
}
