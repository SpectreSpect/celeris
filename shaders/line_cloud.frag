#version 450

layout(location = 0) in vec4 v_color;
layout(location = 0) out vec4 out_color;

vec4 linear_to_srgb(vec4 color) {
    return vec4(pow(color.rgb, vec3(1.0 / 2.2)), color.a);
}

void main()
{
    out_color = linear_to_srgb(v_color);
}
