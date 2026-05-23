#version 450

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 frag_world_pos;
layout(location = 1) out vec3 frag_normal;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    vec4 world_pos = pc.model * vec4(in_position.xyz, 1.0);

    frag_world_pos = world_pos.xyz;

    mat3 normal_matrix = transpose(inverse(mat3(pc.model)));
    frag_normal = normalize(normal_matrix * in_normal.xyz);

    gl_Position = camera_uniform.proj * camera_uniform.view * world_pos;
}