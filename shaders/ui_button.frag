#version 450
layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragColor;

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) out vec4 outColor;

void main() {
    float intensity = texture(tex, fragUV).r;
    outColor = vec4(fragColor.rgb * intensity, fragColor.a);
}
