#version 460

layout(location = 0) in vec4 inPosition;

layout(location = 0) out vec4 outDir;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;


void main() {
    outDir = inPosition;

    mat4 viewRot = mat4(mat3(camera_uniform.view));
    vec4 pos = camera_uniform.proj * viewRot * vec4(inPosition.xyz, 1.0);

    gl_Position = pos.xyww;
}


// #version 430 core

// layout(location = 0) in vec3 aPos;

// out vec3 vDir;

// uniform mat4 uProj;
// uniform mat4 uView;

// void main() {
//     vDir = aPos;

//     mat4 viewRot = mat4(mat3(uView));
//     vec4 pos = uProj * viewRot * vec4(aPos, 1.0);

//     gl_Position = pos.xyww;
// }