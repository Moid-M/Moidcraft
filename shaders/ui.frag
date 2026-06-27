#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(binding = 0) uniform sampler2D fontSampler;

layout(location = 0) out vec4 outColor;

void main() {
    float alpha = texture(fontSampler, fragUV).r;
    if (alpha < 0.01) discard;
    outColor = fragColor;
    outColor.a *= alpha;
}
