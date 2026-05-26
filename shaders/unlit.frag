#version 450

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;

layout(location = 0) out vec4 out_color;

struct MaterialData {
    vec4 color;
};

layout(std430, set = 0, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} material_buffer;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_data_id;
} pc;

vec4 linear_to_srgb(vec4 color)
{
    return vec4(pow(color.rgb, vec3(1.0 / 2.2)), color.a);
}

void main() {
    out_color = linear_to_srgb(material_buffer.materials[pc.material_data_id].color);
}