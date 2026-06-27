#version 450
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} push;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragColor;

void main() {
    vec2 ndc = (inPos / push.screenSize) * 2.0;
    gl_Position = vec4(ndc.x - 1.0, ndc.y - 1.0, 0.0, 1.0);
    fragUV = inUV;
    fragColor = inColor;
}
