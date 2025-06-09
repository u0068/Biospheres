#version 430 core
out vec4 fragColor;

uniform vec2 u_cameraPos; // Camera position in world coordinates
uniform float u_cameraZoom; // Zoom level
uniform vec2 u_mousePos;

vec2 screenToWorld(in vec2 screenPos)
{
    return u_cameraPos + screenPos/u_cameraZoom;
}

void main()
{   
    vec2 worldPos = screenToWorld(gl_FragCoord.xy);
    vec3 color = vec3(fract(worldPos), length(worldPos)/10.);
    
    if (distance(u_mousePos, gl_FragCoord.xy) < 10.0 && distance(u_mousePos, gl_FragCoord.xy) > 8.0) {
        color = vec3(1.0, 1.0, 1.0); // Highlight color if close to mouse position
    }

    // Output to screen
    fragColor = vec4(color, 1.0);
}