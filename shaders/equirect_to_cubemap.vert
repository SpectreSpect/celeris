#version 450

layout(location = 0) in vec4 aPos;
layout(location = 0) out vec3 vDir;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
} ubo;

void main() {
    vDir = aPos.xyz;
    gl_Position = ubo.proj * ubo.view * vec4(aPos.xyz, 1.0);
}