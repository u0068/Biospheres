#version 460 core

// Input interface block from vertex shader
in VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec3 viewDir;
    float sizeRatio;
    float distanceRatio;
    int cellAID;
    int cellBID;
} fs_in;

// Uniforms
uniform vec3 u_lightDirection;
uniform vec3 u_lightColor;
uniform vec3 u_ambientColor;
uniform float u_time;
uniform int u_enableLighting;

// Output
out vec4 fragColor;

// Material properties for organic bridge appearance
const vec3 BRIDGE_BASE_COLOR = vec3(0.85, 0.75, 0.65);  // Warm organic color
const float SPECULAR_STRENGTH = 0.3;
const float SPECULAR_SHININESS = 32.0;
const float AMBIENT_STRENGTH = 0.3;
const float RIM_LIGHT_STRENGTH = 0.2;
const float RIM_LIGHT_POWER = 3.0;

// Subsurface scattering approximation
const vec3 SSS_COLOR = vec3(0.95, 0.85, 0.75);
const float SSS_STRENGTH = 0.15;

void main() {
    // Normalize interpolated normal
    vec3 normal = normalize(fs_in.normal);
    vec3 viewDir = normalize(fs_in.viewDir);
    vec3 lightDir = normalize(-u_lightDirection);
    
    // Base color with slight variation based on size ratio
    // Larger size ratios get slightly warmer tones
    float colorVariation = smoothstep(1.0, 3.0, fs_in.sizeRatio) * 0.1;
    vec3 baseColor = BRIDGE_BASE_COLOR + vec3(colorVariation, colorVariation * 0.5, 0.0);
    
    // Initialize final color
    vec3 finalColor = vec3(0.0);
    
    if (u_enableLighting == 1) {
        // Ambient lighting
        vec3 ambient = AMBIENT_STRENGTH * u_ambientColor * baseColor;
        
        // Diffuse lighting (Lambertian)
        float diffuseIntensity = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = diffuseIntensity * u_lightColor * baseColor;
        
        // Specular lighting (Phong)
        vec3 reflectDir = reflect(-lightDir, normal);
        float specularIntensity = pow(max(dot(viewDir, reflectDir), 0.0), SPECULAR_SHININESS);
        vec3 specular = SPECULAR_STRENGTH * specularIntensity * u_lightColor;
        
        // Rim lighting for organic appearance
        float rimIntensity = pow(1.0 - max(dot(viewDir, normal), 0.0), RIM_LIGHT_POWER);
        vec3 rimLight = RIM_LIGHT_STRENGTH * rimIntensity * u_lightColor;
        
        // Subsurface scattering approximation
        // Light passing through the bridge from behind
        float sssIntensity = max(0.0, dot(normal, -lightDir));
        sssIntensity = pow(sssIntensity, 2.0);
        vec3 sss = SSS_STRENGTH * sssIntensity * SSS_COLOR;
        
        // Combine all lighting components
        finalColor = ambient + diffuse + specular + rimLight + sss;
        
        // Add subtle pulsing effect based on distance ratio
        // Tighter connections (lower distance ratio) pulse more
        float pulseFrequency = mix(2.0, 4.0, 1.0 - smoothstep(0.5, 2.0, fs_in.distanceRatio));
        float pulse = 0.5 + 0.5 * sin(u_time * pulseFrequency);
        float pulseStrength = 0.05 * (1.0 - smoothstep(0.5, 2.0, fs_in.distanceRatio));
        finalColor += pulseStrength * pulse * baseColor;
        
    } else {
        // No lighting - just use base color
        finalColor = baseColor;
    }
    
    // Clamp to valid range
    finalColor = clamp(finalColor, 0.0, 1.0);
    
    // Output final color with full opacity
    fragColor = vec4(finalColor, 1.0);
}
