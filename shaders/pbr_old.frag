#version 450

layout(location = 0) in vec4 outColor;
layout(location = 1) in vec2 outUV;
layout(location = 2) in vec4 outPosition;


layout(location = 0) out vec4 outFragColor;

layout(set = 0, binding = 1) uniform sampler2D albedoTex;
layout(set = 0, binding = 2) uniform samplerCube envMap;
layout(set = 0, binding = 3) uniform samplerCube irradianceMap;

void main() { 
    vec3 dir = normalize(outPosition.xyz);
    vec4 c = texture(irradianceMap, dir);


    // float uExposure = 0.2;
    // vec3 hdrColor = texture(albedoTex, outUV.xy).xyz;

    // // exposure tone mapping
    // vec3 mapped = vec3(1.0) - exp(-hdrColor * uExposure);

    // // gamma correction
    // mapped = pow(mapped, vec3(1.0 / 2.2));

    // if (hdrColor.x < 1.0 || hdrColor.y < 1.0 || hdrColor.z < 1.0)
    //     hdrColor = vec3(0, 0, 0);

    // outFragColor = vec4(hdrColor, 1.0);
    outFragColor = c;
}