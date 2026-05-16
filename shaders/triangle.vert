#version 450

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 frag_color;

layout(push_constant) uniform PushConstants {
    vec2 offset;
    float scale;
} pc;

void main() {
    vec2 pos = in_position;
    pos = pos * pc.scale + pc.offset;

    gl_Position = vec4(pos, 0.0, 1.0);
    frag_color = in_color;
}
