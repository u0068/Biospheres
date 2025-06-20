#version 450 core

in vec3 worldPos;         // World space position
in vec3 localPos;         // Local position relative to ring center

out vec4 FragColor;

void main()
{
    // Use the local Y position to determine coloring (before transformation)
    // This ensures consistent coloring regardless of ring orientation
    // Positive local Y = top (blue), negative local Y = bottom (red)
    
    float localY = localPos.y;
    
    if (localY > 0.0) {
        // Top side - blue
        FragColor = vec4(0.0, 0.0, 1.0, 0.8); // Blue with some transparency
    } else {
        // Bottom side - red  
        FragColor = vec4(1.0, 0.0, 0.0, 0.8); // Red with some transparency
    }
}
