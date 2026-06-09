#version 450

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec4 inNormal;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec2 inUV;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec4 outPosition;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 mvp;
} ubo;

void main() {
    // gl_Position = ubo.proj * ubo.view * ubo.model * inPosition;
    gl_Position = ubo.mvp * vec4(inPosition.xyz, 1.0);
    outColor = inColor;
    outUV = inUV;
    outPosition = inPosition;
}