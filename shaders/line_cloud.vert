#version 450

layout(location = 0) in vec2 a_line_pos;

layout(location = 1) in vec3 i_p0;
layout(location = 2) in vec3 i_p1;
layout(location = 3) in vec4 i_color;

layout(location = 0) out vec4 v_color;

struct MaterialData {
    vec4 color;
    float line_width_pixels;
};

layout(std430, set = 0, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} material_buffer;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    vec2 viewport;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_id;
} pc;

void main()
{
    MaterialData material = material_buffer.materials[pc.material_id];

    vec4 world0 = pc.model * vec4(i_p0, 1.0);
    vec4 world1 = pc.model * vec4(i_p1, 1.0);

    mat4 view_proj = camera_uniform.proj * camera_uniform.view;

    vec4 clip0 = view_proj * world0;
    vec4 clip1 = view_proj * world1;

    vec2 ndc0 = clip0.xy / clip0.w;
    vec2 ndc1 = clip1.xy / clip1.w;

    vec2 screen_size = camera_uniform.viewport;

    vec2 screen0 = (ndc0 * 0.5 + 0.5) * screen_size;
    vec2 screen1 = (ndc1 * 0.5 + 0.5) * screen_size;

    vec2 dir = screen1 - screen0;

    if (dot(dir, dir) < 0.000001) {
        dir = vec2(1.0, 0.0);
    } else {
        dir = normalize(dir);
    }

    vec2 normal = vec2(-dir.y, dir.x);

    float half_width = material.line_width_pixels * 0.5;

    float end_sign = a_line_pos.x < 0.5 ? -1.0 : 1.0;

    vec2 offset_pixels =
        normal * a_line_pos.y * half_width +
        dir * end_sign * half_width;

    vec2 offset_ndc = offset_pixels * vec2(
        2.0 / screen_size.x,
        2.0 / screen_size.y
    );

    vec4 clip = mix(clip0, clip1, a_line_pos.x);
    clip.xy += offset_ndc * clip.w;

    gl_Position = clip;
    v_color = i_color * material.color;
}
