#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 fogColor;
    vec4 cameraPos;
    float time;
} ubo;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 3) in float inLighting;

layout(location = 0) out vec2 fragUv;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out float fragLighting;
layout(location = 3) out vec3 fragWorldPos;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = ubo.proj * ubo.view * worldPos;

    fragUv = inUv;
    fragNormal = mat3(pc.model) * inNormal;
    fragLighting = inLighting;
    fragWorldPos = worldPos.xyz;
}
