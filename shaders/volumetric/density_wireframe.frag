#version 460 core

// Fragment shader for density-based wireframe rendering
// Renders wireframe with color mapping based on density values

// Input from vertex shader
in VS_OUT {
    vec3 worldPos;
    float density;
    float alpha;
} fs_in;

// Uniforms
uniform vec4 u_wireframeColor;
uniform float u_densityThreshold;
uniform float u_maxDensity;
uniform int u_enableColorMapping;
uniform int u_enableWireframe;

// Output
out vec4 fragColor;

// Color mapping function - maps density to color
vec3 densityToColor(float density) {
    if (u_enableColorMapping == 0) {
        return u_wireframeColor.rgb;
    }
    
    // Normalize density to [0, 1] range
    float normalizedDensity = clamp(density / u_maxDensity, 0.0, 1.0);
    
    // Color gradient: blue (low) -> green (medium) -> red (high)
    vec3 lowColor = vec3(0.0, 0.0, 1.0);    // Blue
    vec3 midColor = vec3(0.0, 1.0, 0.0);    // Green
    vec3 highColor = vec3(1.0, 0.0, 0.0);   // Red
    
    if (normalizedDensity < 0.5) {
        // Interpolate between low and mid
        float t = normalizedDensity * 2.0;
        return mix(lowColor, midColor, t);
    } else {
        // Interpolate between mid and high
        float t = (normalizedDensity - 0.5) * 2.0;
        return mix(midColor, highColor, t);
    }
}

void main() {
    if (u_enableWireframe == 0) {
        discard;
    }
    
    // Skip if density is below threshold
    if (fs_in.density < u_densityThreshold) {
        discard;
    }
    
    // Calculate color based on density
    vec3 color = densityToColor(fs_in.density);
    
    // Apply alpha blending
    float alpha = fs_in.alpha * u_wireframeColor.a;
    
    fragColor = vec4(color, alpha);
}