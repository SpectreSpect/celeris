#version 450

layout(location = 0) in vec4 in_position;
layout(location = 1) in uint in_color;
layout(location = 2) in uint in_face;

layout(location = 0) out vec3 frag_world_pos;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec2 frag_uv;
layout(location = 3) out vec3 frag_tangent;
layout(location = 4) out float frag_tangent_sign;
layout(location = 5) out vec3 frag_view_pos;
layout(location = 6) out vec3 frag_voxel_color;
layout(location = 7) out float frag_voxel_ao;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_data_id;
} pc;

vec3 face_normal(uint f) {
    if (f == 0u) return vec3( 1.0,  0.0,  0.0);
    if (f == 1u) return vec3(-1.0,  0.0,  0.0);
    if (f == 2u) return vec3( 0.0,  1.0,  0.0);
    if (f == 3u) return vec3( 0.0, -1.0,  0.0);
    if (f == 4u) return vec3( 0.0,  0.0,  1.0);
    return vec3(0.0, 0.0, -1.0);
}

vec3 face_tangent(uint f) {
    if (f == 0u || f == 1u) return vec3(0.0, 1.0, 0.0);
    if (f == 2u || f == 3u) return vec3(0.0, 0.0, 1.0);
    return vec3(1.0, 0.0, 0.0);
}

vec3 unpack_rgb(uint rgba8) {
    float r = float((rgba8 >> 24) & 0xFFu) / 255.0;
    float g = float((rgba8 >> 16) & 0xFFu) / 255.0;
    float b = float((rgba8 >> 8) & 0xFFu) / 255.0;
    return vec3(r, g, b);
}

void main()
{
    vec4 world_pos = pc.model * vec4(in_position.xyz, 1.0);
    mat3 normal_matrix = transpose(inverse(mat3(pc.model)));

    frag_world_pos = world_pos.xyz;
    frag_normal = normalize(normal_matrix * face_normal(in_face));
    frag_tangent = normalize(normal_matrix * face_tangent(in_face));
    frag_tangent_sign = 1.0;
    frag_uv = in_position.xy;
    frag_view_pos = vec3(camera_uniform.view * world_pos);
    frag_voxel_color = unpack_rgb(in_color);
    frag_voxel_ao = float(in_color & 0xFFu) / 255.0;

    gl_Position = camera_uniform.proj * camera_uniform.view * world_pos;
}
