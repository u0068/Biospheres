#version 430 core

layout(location = 0) in vec3 aPosition;     // Sphere mesh vertex position
layout(location = 1) in vec3 aNormal;       // Sphere mesh vertex normal
layout(location = 2) in vec4 aPositionAndRadius; // x, y, z, radius
layout(location = 3) in vec4 aColor;             // r, g, b, unused
layout(location = 4) in vec4 aOrientation;       // quaternion: w, x, y, z


uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uCameraPos;
uniform vec3 uLightDir;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vCameraPos;
out float vRadius;
out vec3 vInstanceCenter;
out vec3 vBaseColor;


// Convert quaternion to rotation matrix
mat3 quatToMat3(vec4 quat) {
    float x = quat.x, y = quat.y, z = quat.z, w = quat.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    return mat3(
        1.0 - (yy + zz), xy + wz, xz - wy,
        xy - wz, 1.0 - (xx + zz), yz + wx,
        xz + wy, yz - wx, 1.0 - (xx + yy)
    );
}

void main() {
    // Convert quaternion to rotation matrix
    mat3 rotMatrix = quatToMat3(aOrientation);
    
    // Apply rotation to vertex position and normal
    vec3 rotatedPosition = rotMatrix * aPosition;
    vec3 rotatedNormal = rotMatrix * aNormal;
    
    // Scale the rotated sphere vertex by the instance radius and translate by instance position
    vec3 worldPos = aPositionAndRadius.xyz + (rotatedPosition * aPositionAndRadius.w);
    
    // Output data
    vNormal = rotatedNormal;
    vWorldPos = worldPos;
    vCameraPos = uCameraPos;
    vRadius = aPositionAndRadius.w;
    vInstanceCenter = aPositionAndRadius.xyz;
    vBaseColor = aColor.rgb;


    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
} 