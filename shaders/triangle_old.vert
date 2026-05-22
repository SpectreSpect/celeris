#version 450

layout(location = 0) in vec4 in_position;

layout(location = 0) out vec4 frag_color;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
} pc;

void main() {
    vec3 local_pos = in_position.xyz;

    mat4 mvp = camera_uniform.proj * camera_uniform.view * pc.model;

    gl_Position = mvp * vec4(local_pos, 1.0);

    frag_color = vec4(local_pos + vec3(0.5), 1.0);
}