#version 460 core

in vec3 v_worldPos;
in vec4 v_color;
in vec3 v_normal;
in vec4 v_nutrientDensity;
flat in uint v_isActive;

out vec4 FragColor;

uniform vec3 u_cameraPos;
uniform float u_colorSensitivity; // Controls how sensitive colors are to nutrient density

void main()
{
    // Discard inactive voxels
    if (v_isActive == 0) discard;
    
    // Calculate total nutrient density
    float totalNutrient = v_nutrientDensity.r + v_nutrientDensity.g + v_nutrientDensity.b;
    
    // Apply sensitivity scaling
    float scaledDensity = totalNutrient * u_colorSensitivity;
    
    // Normalize individual nutrient channels
    vec3 nutrientRatio = vec3(0.33, 0.33, 0.33); // Default grey if no nutrients
    if (totalNutrient > 0.001)
    {
        nutrientRatio = v_nutrientDensity.rgb / totalNutrient;
    }
    
    // Create color gradient based on density
    // Low density: dark, High density: bright
    float brightness = clamp(scaledDensity, 0.0, 1.0);
    vec3 baseColor = mix(vec3(0.1), nutrientRatio, brightness);
    
    // Simple lighting
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diffuse = max(dot(v_normal, lightDir), 0.0);
    
    // Add ambient and diffuse lighting
    float ambient = 0.4;
    float lighting = ambient + diffuse * 0.6;
    
    // Apply lighting to color
    vec3 litColor = baseColor * lighting;
    
    // Opaque rendering - no transparency
    FragColor = vec4(litColor, 1.0);
}
