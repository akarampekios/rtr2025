#version 460 core
#extension GL_EXT_ray_tracing : require

struct RayPayload {
    vec3 color;
    vec3 normal;
    vec3 worldPos;
    float hitDistance;
    int bounceCount;
};

hitAttributeEXT vec2 attribs;
layout(location = 0) rayPayloadInEXT RayPayload payload;

layout(set = 0, binding = 4) readonly buffer VertexBuffer {
    float data[];
} vertexBuffer;

layout(set = 0, binding = 5) readonly buffer IndexBuffer {
    uint indices[];
} indexBuffer;

// Get vertex data from buffers
const uint VERTEX_STRIDE = 11u; // position(3) + normal(3) + texCoord(2) + color(3)

vec3 getVertexPosition(uint index) {
    uint base = index * VERTEX_STRIDE;
    return vec3(vertexBuffer.data[base + 0],
                vertexBuffer.data[base + 1],
                vertexBuffer.data[base + 2]);
}

vec3 getVertexNormal(uint index) {
    uint base = index * VERTEX_STRIDE + 3;
    vec3 normal = vec3(vertexBuffer.data[base + 0],
                      vertexBuffer.data[base + 1],
                      vertexBuffer.data[base + 2]);
    return normalize(normal);
}

vec3 getVertexColor(uint index) {
    uint base = index * VERTEX_STRIDE + 8;
    return vec3(vertexBuffer.data[base + 0],
                vertexBuffer.data[base + 1],
                vertexBuffer.data[base + 2]);
}

void main() {
    // Get triangle indices
    uint primitiveIndex = gl_PrimitiveID;
    uint indexOffset = primitiveIndex * 3;
    
    uint i0 = indexBuffer.indices[indexOffset + 0];
    uint i1 = indexBuffer.indices[indexOffset + 1];
    uint i2 = indexBuffer.indices[indexOffset + 2];
    
    // Get vertex positions
    vec3 v0 = getVertexPosition(i0);
    vec3 v1 = getVertexPosition(i1);
    vec3 v2 = getVertexPosition(i2);
    
    // Interpolate position using barycentric coordinates
    vec3 barycentric = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 worldPos = v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
    
    // Calculate normal (cross product of triangle edges)
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 normal = normalize(cross(edge1, edge2));
    
    // Interpolate attributes
    vec3 color0 = getVertexColor(i0);
    vec3 color1 = getVertexColor(i1);
    vec3 color2 = getVertexColor(i2);
    vec3 interpolatedColor = color0 * barycentric.x + color1 * barycentric.y + color2 * barycentric.z;

    vec3 normal0 = getVertexNormal(i0);
    vec3 normal1 = getVertexNormal(i1);
    vec3 normal2 = getVertexNormal(i2);
    vec3 interpolatedNormal = normalize(normal0 * barycentric.x + normal1 * barycentric.y + normal2 * barycentric.z);

    payload.color = interpolatedColor;
    payload.normal = interpolatedNormal;
    payload.worldPos = worldPos;
    payload.hitDistance = gl_HitTEXT;
    payload.bounceCount = 0; // Will be set by ray generation shader
}
