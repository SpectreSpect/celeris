#version 450

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    vec4 offset; // xyz = object position, w unused
    float scale;
} pc;

void main() {
    vec3 local_pos = in_position.xyz;

    vec3 world_pos = local_pos * pc.scale + pc.offset.xyz;

    gl_Position = camera_uniform.proj * camera_uniform.view * vec4(world_pos, 1.0);;

    frag_color = vec4(local_pos + vec3(0.5), 1.0);
}