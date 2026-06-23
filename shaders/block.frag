#version 450

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in float fragLighting;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    vec4 fogColor;
    vec4 cameraPos;
    float time;
} ubo;

layout(binding = 1) uniform sampler2D blockTexture;

void main() {
    vec3 texColor = texture(blockTexture, fragUv).rgb;

    if (texColor.r < 0.01 && texColor.g < 0.01 && texColor.b < 0.01) {
        texColor = vec3(1.0, 0.0, 1.0);
    }

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(fragNormal), lightDir), 0.2);
    diff *= fragLighting;

    outColor = vec4(texColor * diff, 1.0);

    vec3 viewPos = ubo.cameraPos.xyz;
    float dist = length(fragWorldPos - viewPos);
    float fog = 1.0 - exp(-dist * dist * 0.00005);
    outColor = mix(outColor, ubo.fogColor, clamp(fog, 0.0, 0.8));
}
