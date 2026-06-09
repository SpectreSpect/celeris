#version 450

layout(location = 0) in vec3 vDir;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 1) uniform sampler2D uEquirectangularMap;

const vec2 invAtan = vec2(0.15915494309, 0.31830988618);

vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(clamp(v.y, -1.0, 1.0)));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec3 dir = normalize(vDir);
    vec2 uv = sampleSphericalMap(dir);
    vec3 color = texture(uEquirectangularMap, uv).rgb;
    FragColor = vec4(color, 1.0);
}