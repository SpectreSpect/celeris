#version 450

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec4 frag_color;
layout(location = 2) in vec3 frag_normal;
layout(location = 3) in vec3 frag_view_pos;

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

vec3 linear_to_srgb(vec3 color) {
    color = max(color, vec3(0.0));
    return pow(color, vec3(1.0 / 2.2));
}

void main() {
    vec3 normal = normalize(frag_normal);
    vec3 light_dir = normalize(vec3(0.35, 0.85, 0.45));

    float diffuse = max(dot(normal, light_dir), 0.0);
    float shade = 0.30 + 0.70 * diffuse;

    vec4 material = material_buffer.materials[pc.material_data_id].color;
    vec3 color = frag_color.rgb * material.rgb * shade;

    float distance_fog = clamp((length(frag_view_pos) - 60.0) / 240.0, 0.0, 1.0);
    vec3 fog_color = vec3(0.05, 0.06, 0.07);
    color = mix(color, fog_color, distance_fog);

    out_color = vec4(linear_to_srgb(color), frag_color.a * material.a);
}
