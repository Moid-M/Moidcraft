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
    float gamma;
    float alphaDiscard;
} ubo;

layout(binding = 1) uniform sampler2D blockTexture;

void main() {
    vec4 texColor = texture(blockTexture, fragUv);

    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(normalize(fragNormal), lightDir), 0.2);
    diff *= fragLighting;

    if (texColor.a < ubo.alphaDiscard) discard;

    outColor = vec4(texColor.rgb * diff, texColor.a);

    outColor.rgb = pow(outColor.rgb, vec3(1.0 / ubo.gamma));

    vec3 viewPos = ubo.cameraPos.xyz;
    float dist = length(fragWorldPos - viewPos);
    float fog = 1.0 - exp(-dist * dist * 0.00005);
    outColor = vec4(mix(outColor.rgb, ubo.fogColor.rgb, clamp(fog, 0.0, 0.8)), outColor.a);
}
