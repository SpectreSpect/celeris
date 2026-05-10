#version 460

layout(location = 0) in vec4 vColor;

layout(location = 0) out vec4 FragColor;

void main() {
    vec4 col = vColor;

    if (col.a <= 0.001) {
        discard;
    }

    vec3 color = pow(col.rgb, vec3(1.0 / 2.2));

    FragColor = vec4(color, col.a);
}