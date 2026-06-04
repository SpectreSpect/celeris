#version 450

layout(location = 0) in vec4 in_position;
layout(location = 1) in uint in_color;
layout(location = 2) in uint in_face;

layout(location = 0) out vec3 frag_world_pos;
layout(location = 1) out vec4 frag_color;
layout(location = 2) out vec3 frag_normal;
layout(location = 3) out vec3 frag_view_pos;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_data_id;
} pc;

vec4 unpack_color(uint packed_color) {
    float r = float((packed_color >> 24u) & 0xFFu) / 255.0;
    float g = float((packed_color >> 16u) & 0xFFu) / 255.0;
    float b = float((packed_color >> 8u) & 0xFFu) / 255.0;
    float a = float(packed_color & 0xFFu) / 255.0;
    return vec4(r, g, b, a);
}

vec3 face_normal(uint face) {
    if (face == 0u) return vec3( 1.0,  0.0,  0.0);
    if (face == 1u) return vec3(-1.0,  0.0,  0.0);
    if (face == 2u) return vec3( 0.0,  1.0,  0.0);
    if (face == 3u) return vec3( 0.0, -1.0,  0.0);
    if (face == 4u) return vec3( 0.0,  0.0,  1.0);
    return vec3(0.0, 0.0, -1.0);
}

void main() {
    vec4 world_pos = pc.model * vec4(in_position.xyz, 1.0);
    mat3 normal_matrix = transpose(inverse(mat3(pc.model)));

    frag_world_pos = world_pos.xyz;
    frag_color = unpack_color(in_color);
    frag_normal = normalize(normal_matrix * face_normal(in_face));
    frag_view_pos = vec3(camera_uniform.view * world_pos);

    gl_Position = camera_uniform.proj * camera_uniform.view * world_pos;
}
