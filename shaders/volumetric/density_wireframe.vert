#version 460 core

// Vertex shader for density-based wireframe rendering
// Renders wireframe cubes around voxels containing density

layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_instancePosition;
layout (location = 2) in float a_instanceDensity;

// Uniforms
uniform mat4 u_modelMatrix;
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform float u_voxelSize;
uniform float u_densityThreshold;
uniform int u_enableWireframe;

// Output to fragment shader
out VS_OUT {
    vec3 worldPos;
    float density;
    float alpha;
} vs_out;

void main() {
    if (u_enableWireframe == 0) {
        gl_Position = vec4(0.0);
        return;
    }
    
    // Skip if density is below threshold
    if (a_instanceDensity < u_densityThreshold) {
        gl_Position = vec4(0.0);
        return;
    }
    
    // Scale vertex position by voxel size and translate to instance position
    vec3 scaledPos = a_position * u_voxelSize;
    vec3 worldPos = a_instancePosition + scaledPos;
    
    // Transform to clip space
    vec4 viewPos = u_viewMatrix * u_modelMatrix * vec4(worldPos, 1.0);
    gl_Position = u_projectionMatrix * viewPos;
    
    // Pass data to fragment shader
    vs_out.worldPos = worldPos;
    vs_out.density = a_instanceDensity;
    
    // Calculate alpha based on density (higher density = more opaque)
    vs_out.alpha = clamp(a_instanceDensity, 0.1, 1.0);
}