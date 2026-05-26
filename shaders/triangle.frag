#version 450

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;

layout(location = 0) out vec4 out_color;

struct MaterialData {
    vec4 material;
    vec4 color;
};

layout(std430, set = 0, binding = 0) readonly buffer MaterialBuffer {
    MaterialData materials[];
} material_buffer;

// layout(set = 0, binding = 1) uniform UniformBufferObject {
//     vec4 color;

//     // x = ambient strength
//     // y = diffuse strength
//     // z = specular strength
//     // w = shininess
//     vec4 material;
// } ubo;

layout(set = 0, binding = 1) uniform sampler2D texSampler;

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
    vec3 rgb = max(color.rgb, vec3(0.0));

    vec3 low  = rgb * 12.92;
    vec3 high = 1.055 * pow(rgb, vec3(1.0 / 2.4)) - 0.055;

    vec3 srgb = mix(low, high, lessThanEqual(rgb, vec3(0.0031308)));

    return vec4(srgb, color.a);
}


void main() {
    // vec3 base_color = ubo.color.rgb;
    MaterialData material_data = material_buffer.materials[pc.material_data_id];

    vec3 base_color = texture(texSampler, frag_uv).xyz * material_data.color.xyz;

    vec4 material = material_data.material;

    float ambient_strength  = material.x;
    float diffuse_strength  = material.y;
    float specular_strength = material.z;
    float shininess         = max(material.w, 1.0);

    vec3 N = normalize(frag_normal);
    vec3 V = normalize(camera_uniform.viewPos.xyz - frag_world_pos);

    // Temporary light direction.
    // This makes the camera act like a light source.
    vec3 L = normalize(vec3(-1, 0.5, -1));

    vec3 H = normalize(L + V);

    float diffuse = max(dot(N, L), 0.0);

    float specular = 0.0;
    if (diffuse > 0.0) {
        specular = pow(max(dot(N, H), 0.0), shininess);
    }

    vec3 ambient  = ambient_strength * base_color;
    vec3 diffuse_c = diffuse_strength * diffuse * base_color;
    vec3 specular_c = specular_strength * specular * vec3(1.0);

    vec3 final_color = ambient + diffuse_c + specular_c;

    out_color = linear_to_srgb(vec4(final_color, 1));
    // out_color = ;
}
