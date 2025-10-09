#version 460 core

in float v_nutrientDensity;
in vec2 v_texCoord;
in float v_distanceFade;

out vec4 FragColor;

uniform float u_colorSensitivity;

void main()
{
    // Create circular particle shape
    vec2 centerOffset = v_texCoord - vec2(0.5);
    float dist = length(centerOffset);
    
    // Soft circle with smooth edges
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
    
    // Early discard for fully transparent pixels to reduce overdraw
    if (alpha < 0.01) discard;
    
    // Color based on nutrient density
    float scaledDensity = v_nutrientDensity * u_colorSensitivity;
    float brightness = clamp(scaledDensity, 0.0, 1.0);
    
    // Gradient from cyan (low density) to green (high density)
    vec3 lowColor = vec3(0.2, 0.8, 0.9);   // Cyan for low nutrients
    vec3 highColor = vec3(0.2, 1.0, 0.3);  // Bright green for high nutrients
    vec3 color = mix(lowColor, highColor, brightness);
    
    // Add glow effect
    float glow = 1.0 - dist * 2.0;
    glow = max(0.0, glow);
    color += vec3(0.5, 0.7, 1.0) * glow * brightness * 0.3;
    
    // Simple alpha with distance fade only (no density fade)
    float finalAlpha = alpha * 0.7 * v_distanceFade;
    
    FragColor = vec4(color, finalAlpha);
}
