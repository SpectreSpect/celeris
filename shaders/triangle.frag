#version 450

layout(location = 0) in vec3 frag_color;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    vec4 color;
} ubo;

// layout(std430, set = 0, binding = 1) readonly buffer SimpleStorageBuffer {
//     vec4 color1;
//     vec4 color2;
// } simple_storage_buffer;


void main() {
    // out_color = vec4(frag_color, 1.0);
    out_color = ubo.color;
}