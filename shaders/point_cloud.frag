#version 450

layout(location = 0) in vec2 vLocal;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 FragColor;

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
    vec3 rgb = max(color.rgb, vec3(0.0));

    vec3 low  = rgb * 12.92;
    vec3 high = 1.055 * pow(rgb, vec3(1.0 / 2.4)) - 0.055;

    vec3 srgb = mix(low, high, lessThanEqual(rgb, vec3(0.0031308)));

    return vec4(srgb, color.a);
}


void main() {
    vec4 col = vColor;

    float r = length(vLocal);
    float aa = fwidth(r);
    float alpha = 1.0 - smoothstep(1.0 - aa, 1.0 + aa, r);
    col.a *= alpha;

    if (col.a <= 0.001) {
        discard;
    }

    vec3 color = pow(col.xyz, vec3(1.0 / 2.2));

    FragColor = linear_to_srgb(vec4(color, 1.0));
}