#version 460 core

// Vertex attributes from mesh data
layout (location = 0) in vec3 a_position;
layout (location = 1) in vec3 a_normal;

// Instance data from bridge instance buffer
layout (location = 2) in vec3 a_instancePosition;
layout (location = 3) in float a_instanceScale;
layout (location = 4) in vec3 a_instanceOrientation;
layout (location = 5) in float a_instanceDistance;
layout (location = 6) in float a_sizeRatio;
layout (location = 7) in float a_distanceRatio;
layout (location = 8) in int a_cellAID;
layout (location = 9) in int a_cellBID;

// Uniforms
uniform mat4 u_viewMatrix;
uniform mat4 u_projectionMatrix;
uniform vec3 u_cameraPosition;

// Mesh library uniforms for bilinear interpolation
// We'll use texture buffers to store multiple mesh variations
// For now, we'll implement a simplified version that uses the base mesh
// and applies transformations. Full bilinear interpolation will be added
// when the mesh library system is implemented.

// Output interface block
out VS_OUT {
    vec3 worldPos;
    vec3 normal;
    vec3 viewDir;
    float sizeRatio;
    float distanceRatio;
    int cellAID;
    int cellBID;
} vs_out;

// Helper function to create rotation matrix from direction vector
mat3 createRotationMatrix(vec3 direction) {
    // Create rotation matrix that aligns Z-axis with the direction vector
    vec3 up = abs(direction.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, direction));
    up = normalize(cross(direction, right));
    
    return mat3(right, up, direction);
}

void main() {
    // TODO: Implement bilinear interpolation between 4 mesh variations
    // For now, we use the base mesh and apply instance transformations
    // This will be enhanced when the mesh library system is implemented
    
    // Create rotation matrix from orientation vector
    mat3 rotationMatrix = createRotationMatrix(a_instanceOrientation);
    
    // Apply instance transformation:
    // 1. Scale the mesh vertex by instance scale
    vec3 scaledPosition = a_position * a_instanceScale;
    
    // 2. Rotate to align with bridge orientation
    vec3 rotatedPosition = rotationMatrix * scaledPosition;
    
    // 3. Translate to bridge midpoint position
    vec3 worldPosition = a_instancePosition + rotatedPosition;
    
    // Transform normal
    vec3 rotatedNormal = rotationMatrix * a_normal;
    vec3 worldNormal = normalize(rotatedNormal);
    
    // Calculate view direction
    vec3 viewDirection = normalize(u_cameraPosition - worldPosition);
    
    // Output to fragment shader
    vs_out.worldPos = worldPosition;
    vs_out.normal = worldNormal;
    vs_out.viewDir = viewDirection;
    vs_out.sizeRatio = a_sizeRatio;
    vs_out.distanceRatio = a_distanceRatio;
    vs_out.cellAID = a_cellAID;
    vs_out.cellBID = a_cellBID;
    
    // Calculate final position
    gl_Position = u_projectionMatrix * u_viewMatrix * vec4(worldPosition, 1.0);
}
