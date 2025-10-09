#version 460 core

layout(location = 0) in vec4 a_particleData; // xyz = position, w = nutrient density

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_cameraPos;
uniform float u_particleSize;
uniform float u_cullDistance;
uniform float u_fadeStartDistance;

out float v_nutrientDensity;
out vec2 v_texCoord;
out float v_distanceFade;

void main()
{
    // a_particleData is instanced (one per particle), gl_VertexID is the corner (0-3)
    vec3 a_particlePos = a_particleData.xyz;
    float a_nutrientDensity = a_particleData.w;
    
    // Only clip particles with extremely low nutrients (below fade threshold)
    // Fragment shader handles smooth fading from 0.001 to 0.02
    if (a_nutrientDensity < 0.0001)
    {
        gl_Position = vec4(0.0, 0.0, -10.0, 1.0); // Clip
        return;
    }
    
    // Calculate distance from camera
    float distance = length(u_cameraPos - a_particlePos);
    
    // Distance-based culling (same as cells)
    if (distance > u_cullDistance)
    {
        gl_Position = vec4(0.0, 0.0, -10.0, 1.0); // Clip
        return;
    }
    
    // Calculate distance fade (fade out between fadeStart and cullDistance)
    v_distanceFade = 1.0 - smoothstep(u_fadeStartDistance, u_cullDistance, distance);
    
    v_nutrientDensity = a_nutrientDensity;
    
    // Billboard vertices (quad corners) - triangle strip order
    vec2 quadVertices[4] = vec2[4](
        vec2(-1.0, -1.0),  // 0: bottom-left
        vec2( 1.0, -1.0),  // 1: bottom-right
        vec2(-1.0,  1.0),  // 2: top-left
        vec2( 1.0,  1.0)   // 3: top-right
    );
    
    vec2 quadTexCoords[4] = vec2[4](
        vec2(0.0, 0.0),
        vec2(1.0, 0.0),
        vec2(0.0, 1.0),
        vec2(1.0, 1.0)
    );
    
    // Get the corner for this vertex (gl_VertexID = 0-3 for triangle strip)
    vec2 corner = quadVertices[gl_VertexID];
    v_texCoord = quadTexCoords[gl_VertexID];
    
    // Calculate billboard orientation (always face camera)
    vec3 toCamera = normalize(u_cameraPos - a_particlePos);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), toCamera));
    vec3 up = cross(toCamera, right);
    
    // Scale by particle size
    vec3 worldPos = a_particlePos + (right * corner.x + up * corner.y) * u_particleSize;
    
    gl_Position = u_projection * u_view * vec4(worldPos, 1.0);
}
