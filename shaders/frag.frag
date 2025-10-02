#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
    float time;
} ubo;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main() {
    // Basic lighting
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 normal = normalize(fragNormal);
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Ambient lighting
    vec3 ambient = vec3(0.1, 0.1, 0.2);
    
    // Diffuse lighting
    vec3 diffuse = diff * vec3(0.8, 0.8, 1.0);
    
    // Combine lighting
    vec3 lighting = ambient + diffuse;
    
    // Apply cyberpunk color grading
    vec3 color = fragColor * lighting;
    
    // Add some neon glow effect for bright colors
    if (fragColor.r > 0.8 || fragColor.g > 0.8 || fragColor.b > 0.8) {
        float glow = sin(ubo.time * 2.0) * 0.3 + 0.7;
        color += fragColor * glow * 0.5;
    }
    
    outColor = vec4(color, 1.0);
}

