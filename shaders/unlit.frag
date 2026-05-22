#version 450

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;

layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    vec4 color;
} ubo;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

void main() {
    vec3 base_color = ubo.color.rgb;

    out_color = vec4(base_color, ubo.color.a);
}