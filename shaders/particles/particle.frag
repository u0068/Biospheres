#version 460 core

in vec2 TexCoord;
in vec4 ParticleColor;
in float Lifetime;
in float MaxLifetime;
in float FadeFactor;

out vec4 FragColor;

void main() {
    // Create circular particle shape
    vec2 center = vec2(0.5, 0.5);
    float dist = distance(TexCoord, center);
    
    // Soft edge falloff
    float alpha = 1.0 - smoothstep(0.3, 0.5, dist);
    
    // Calculate lifetime-based fade (fade in and fade out)
    float lifetimeRatio = Lifetime / MaxLifetime;
    float fadeTime = 0.2; // 20% of lifetime for fade in/out
    
    float lifetimeAlpha;
    if (lifetimeRatio < fadeTime) {
        // Fade in
        lifetimeAlpha = lifetimeRatio / fadeTime;
    } else if (lifetimeRatio > 1.0 - fadeTime) {
        // Fade out
        lifetimeAlpha = (1.0 - lifetimeRatio) / fadeTime;
    } else {
        // Full opacity during middle life
        lifetimeAlpha = 1.0;
    }
    
    // Apply all alpha factors: color, shape, lifetime, and distance
    float finalAlpha = ParticleColor.a * alpha * lifetimeAlpha * FadeFactor;
    
    // Apply color and fade
    FragColor = vec4(ParticleColor.rgb, finalAlpha);
    
    // Discard fully transparent fragments
    if (FragColor.a < 0.01) {
        discard;
    }
}
