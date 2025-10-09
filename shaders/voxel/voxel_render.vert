#version 460 core

layout(location = 0) in vec3 a_position;

struct VoxelData
{
    vec4 nutrientDensity;
    vec4 positionAndSize;
    vec4 colorAndAlpha;
    float lifetime;
    float maxLifetime;
    uint isActive;
    uint _padding;
};

layout(std430, binding = 0) buffer VoxelBuffer
{
    VoxelData voxels[];
};

layout(std430, binding = 1) buffer ActiveVoxelIndices
{
    uint activeIndices[];
};

uniform mat4 u_view;
uniform mat4 u_projection;
uniform vec3 u_cameraPos;

out vec3 v_worldPos;
out vec4 v_color;
out vec3 v_normal;
out vec4 v_nutrientDensity;
flat out uint v_isActive;

void main()
{
    // Get the actual voxel index from the active indices array
    uint voxelIndex = activeIndices[gl_InstanceID];
    
    // Get voxel data for this instance
    VoxelData voxel = voxels[voxelIndex];
    
    // Skip inactive voxels
    v_isActive = voxel.isActive;
    if (voxel.isActive == 0)
    {
        gl_Position = vec4(0.0, 0.0, -10.0, 1.0); // Clip this vertex
        return;
    }
    
    // Calculate world position
    vec3 voxelCenter = voxel.positionAndSize.xyz;
    float voxelSize = voxel.positionAndSize.w;
    
    // Scale vertex position by voxel size and translate to voxel center
    vec3 worldPos = voxelCenter + a_position * voxelSize;
    v_worldPos = worldPos;
    
    // Pass color and alpha from voxel data
    v_color = voxel.colorAndAlpha;
    
    // Pass nutrient density for color visualization
    v_nutrientDensity = voxel.nutrientDensity;
    
    // Calculate normal (for lighting - just use vertex position as normal for cube)
    v_normal = normalize(a_position);
    
    // Transform to clip space
    gl_Position = u_projection * u_view * vec4(worldPos, 1.0);
}
