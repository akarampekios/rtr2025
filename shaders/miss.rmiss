#version 460 core
#extension GL_EXT_ray_tracing : require

struct RayPayload {
    vec3 color;
    vec3 normal;
    vec3 worldPos;
    float hitDistance;
    int bounceCount;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(set = 0, binding = 2) uniform CameraUBO {
    mat4 viewInverse;
    mat4 projInverse;
    vec3 cameraPos;
    float time;
} cameraUBO;

void main() {
    vec3 rayDir = normalize(gl_WorldRayDirectionEXT);

    vec3 skyTop = vec3(0.0, 0.05, 0.1);
    vec3 skyHorizon = vec3(0.05, 0.02, 0.1);
    float t = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
    vec3 skyColor = mix(skyHorizon, skyTop, smoothstep(0.0, 1.0, t));

    float stars = pow(max(0.0, 1.0 - t), 6.0);
    float starNoise = fract(sin(dot(rayDir.xy, vec2(12.9898, 78.233))) * 43758.5453);
    skyColor += stars * starNoise * vec3(0.8, 0.5, 1.0);

    payload.color = skyColor;
    payload.normal = vec3(0.0, 1.0, 0.0);
    payload.worldPos = vec3(0.0);
    payload.hitDistance = 0.0;
    payload.bounceCount = 0;
}
