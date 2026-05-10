#version 450

layout(location = 0) in vec2 aCorner;
layout(location = 1) in vec4 aPos;
layout(location = 2) in vec4 aColor;

layout(location = 0) out vec2 vLocal;
layout(location = 1) out vec4 vColor;

// layout(std140, set = 0, binding = 0) uniform PBRUniformBuffer {
//     mat4 model;
//     mat4 view;
//     mat4 proj;

//     vec2 viewport;
    
//     float point_size_px;
//     float point_size_world;
//     int screen_space_size;
// } ubo;

layout(std140, set = 0, binding = 0) uniform PBRUniformBuffer {
    // mat4 model;
    mat4 view;
    mat4 proj;

    vec2 viewport;
    
    // float point_size_px;
    // float point_size_world;
    // int screen_space_size;
} ubo;

layout(push_constant) uniform ObjectPushConstants {
    vec4 color;
    mat4 model;
    float point_size_px;
    float point_size_world;
    int screen_space_size;
} pc;


void main() {
    vLocal = aCorner;
    // vColor = aColor;
    vColor = pc.color;

    vec4 centerW4 = pc.model * vec4(aPos.xyz, 1.0);

    if (pc.screen_space_size == 1) {
        vec4 c = ubo.proj * ubo.view * centerW4;

        vec2 pxToNdc = vec2(2.0 / ubo.viewport.x, 2.0 / ubo.viewport.y);
        float radiusPx = 0.5 * pc.point_size_px;
        vec2 offsetNdc = aCorner * radiusPx * pxToNdc;

        c.xy += offsetNdc * c.w;
        gl_Position = c;
    } else {
        vec3 rightW = vec3(ubo.view[0][0], ubo.view[1][0], ubo.view[2][0]);
        vec3 upW    = vec3(ubo.view[0][1], ubo.view[1][1], ubo.view[2][1]);

        float radiusW = 0.5 * pc.point_size_world;

        vec3 centerW = centerW4.xyz;
        vec3 posW = centerW
                  + rightW * (aCorner.x * radiusW)
                  + upW    * (aCorner.y * radiusW);

        gl_Position = ubo.proj * ubo.view * vec4(posW, 1.0);
    }
}