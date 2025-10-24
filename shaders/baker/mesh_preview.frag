#version 460 core

in vec3 v_worldPos;
in vec3 v_normal;

uniform vec3 u_lightDir;
uniform vec3 u_viewPos;
uniform vec3 u_meshColor;

out vec4 fragColor;

void main()
{
    // Normalize interpolated normal
    vec3 normal = normalize(v_normal);
    
    // DEBUG: Visualize normals as colors (uncomment to debug)
    // fragColor = vec4(normal * 0.5 + 0.5, 1.0);
    // return;
    
    // Diffuse lighting
    float diff = max(dot(normal, u_lightDir), 0.0);
    vec3 diffuse = diff * u_meshColor;
    
    // Ambient lighting
    vec3 ambient = 0.3 * u_meshColor;
    
    // Specular lighting
    vec3 viewDir = normalize(u_viewPos - v_worldPos);
    vec3 halfDir = normalize(u_lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), 32.0);
    vec3 specular = vec3(0.3) * spec;
    
    // Combine lighting
    vec3 result = ambient + diffuse + specular;
    
    fragColor = vec4(result, 1.0);
}
