#version 460

layout(location = 0) in vec4 inDir;

layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 proj;
    mat4 view;
    float exposure;
} ubo;

layout(set = 0, binding = 1) uniform samplerCube envMap;

layout(set = 0, binding = 1) uniform samplerCube textures[];

void main() { 
    vec3 dir = normalize(inDir.xyz);
    vec3 hdr_color = texture(envMap, dir).rgb;

    vec3 mapped = vec3(1.0) - exp(-hdr_color * ubo.exposure);

    // outFragColor = texture(albedoTex, outUV.xy);

    // if (hdr_color.x < 1.0 || hdr_color.y < 1.0 || hdr_color.z < 1.0)
    //     hdr_color = vec3(0, 0, 0);

    // mapped = pow(mapped, vec3(1.0 / 2.2));

    outFragColor = vec4(mapped, 1.0);
    // outFragColor = vec4(hdr_color, 1.0);
}


// uniform samplerCube uSkybox;
// uniform float uExposure = 0.2;

// void main() {
//     vec3 hdrColor = texture(uSkybox, normalize(vDir)).rgb;

//     // exposure tone mapping
//     vec3 mapped = vec3(1.0) - exp(-hdrColor * uExposure);

//     // gamma correction
//     mapped = pow(mapped, vec3(1.0 / 2.2));

//     FragColor = vec4(mapped, 1.0);
//     // FragColor = vec4(1, 1, 1, 1.0);
// }