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

layout(set = 0, binding = 1) uniform PointUniform {
    float point_size_px;
    float point_size_world;
    int screen_space_size;
    float pad;
} point_uniform;


layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
    vec2 viewport;
} camera_uniform;

layout(push_constant) uniform ObjectPushConstants {
    mat4 model;
    uint material_id;
} pc;


void main() {
    // float point_size_px = 5;
    // float point_size_world = 5;
    // int screen_space_size = 1;

    vLocal = aCorner;
    vColor = aColor;
    // vColor = pc.color;

    vec4 centerW4 = pc.model * vec4(aPos.xyz, 1.0);

    if (point_uniform.screen_space_size == 1) {
        vec4 c = camera_uniform.proj * camera_uniform.view * centerW4;

        vec2 pxToNdc = vec2(2.0 / camera_uniform.viewport.x, 2.0 / camera_uniform.viewport.y);
        float radiusPx = 0.5 * point_uniform.point_size_px;
        vec2 offsetNdc = aCorner * radiusPx * pxToNdc;

        c.xy += offsetNdc * c.w;
        gl_Position = c;
    } else {
        vec3 rightW = vec3(camera_uniform.view[0][0], camera_uniform.view[1][0], camera_uniform.view[2][0]);
        vec3 upW    = vec3(camera_uniform.view[0][1], camera_uniform.view[1][1], camera_uniform.view[2][1]);

        float radiusW = 0.5 * point_uniform.point_size_world;

        vec3 centerW = centerW4.xyz;
        vec3 posW = centerW
                  + rightW * (aCorner.x * radiusW)
                  + upW    * (aCorner.y * radiusW);

        gl_Position = camera_uniform.proj * camera_uniform.view * vec4(posW, 1.0);
    }
}