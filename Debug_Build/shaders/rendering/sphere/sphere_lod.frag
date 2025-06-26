#version 430 core

in vec3 vWorldPos;
in vec3 vNormal;
in vec3 vCameraPos;
in float vRadius;
in vec3 vInstanceCenter;
in vec3 vBaseColor;


uniform vec3 uLightDir;
uniform vec3 uSelectedCellPos;
uniform float uSelectedCellRadius;
uniform float uTime;

out vec4 FragColor;

void main() {
    vec3 normal = normalize(vNormal);
    vec3 lightDir = normalize(uLightDir);
    
    // Base diffuse lighting
    float diffuse = max(dot(normal, lightDir), 0.1);
    
    // Standard Phong shading (LOD handled by mesh complexity)
    vec3 viewDir = normalize(vCameraPos - vWorldPos);
    vec3 reflectDir = reflect(-lightDir, normal);
    float specular = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    
    vec3 ambient = 0.3 * vBaseColor;
    vec3 diffuseColor = diffuse * vBaseColor;
    vec3 specularColor = specular * vec3(0.5);
    
    vec3 finalColor = ambient + diffuseColor + specularColor;
    
    // Selection highlighting
    float distToSelected = distance(vInstanceCenter, uSelectedCellPos);
    if (distToSelected < uSelectedCellRadius + 0.1) {
        float pulseIntensity = 0.5 + 0.5 * sin(uTime * 8.0);
        finalColor = mix(finalColor, vec3(1.0, 1.0, 0.0), pulseIntensity * 0.3);
    }
    
    FragColor = vec4(finalColor, 1.0);
} 