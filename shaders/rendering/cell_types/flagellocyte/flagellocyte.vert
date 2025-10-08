#version 430 core

layout(location = 0) in vec3 aPosition;     // Sphere mesh vertex position
layout(location = 1) in vec3 aNormal;       // Sphere mesh vertex normal
layout(location = 2) in vec4 aPositionAndRadius; // x, y, z, radius (INSTANCE)
layout(location = 3) in vec4 aColor;             // r, g, b, a (INSTANCE)
layout(location = 4) in vec4 aOrientation;       // quaternion: w, x, y, z (INSTANCE)
layout(location = 5) in vec4 aFadeFactor;        // fade factor (INSTANCE)

uniform mat4 uProjection;
uniform mat4 uView;

out vec3 vWorldPos;
out vec3 vNormal;
out vec3 vColor;
out vec3 vInstanceCenter;
out float vFadeFactor;

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
    
    // Scale and translate
    vec3 worldPos = aPositionAndRadius.xyz + (rotatedPosition * aPositionAndRadius.w);
    
    vWorldPos = worldPos;
    vNormal = rotatedNormal;
    vColor = aColor.rgb;
    vInstanceCenter = aPositionAndRadius.xyz;
    vFadeFactor = aFadeFactor.x;

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
