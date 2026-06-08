#version 460

layout(location = 0) in vec4 aPos;      // из vb[].pos (vec4)
layout(location = 1) in uint aColor;    // из vb[].color (RGBA8)
layout(location = 2) in uint aFace;     // из vb[].face (0..5)

// uniform mat4 uWorld;
// uniform mat4 uProjView;

layout(set = 1, binding = 0) uniform CameraUniform {
    mat4 view;
    mat4 proj;
    vec4 viewPos;
} camera_uniform;

layout(push_constant) uniform PushConstants {
    mat4 model;
    uint material_data_id;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vFragPos;
layout(location = 2) out vec3 vColor;
layout(location = 3) out float vAO;

vec3 face_normal(uint f) {
    // face: 0=+X,1=-X,2=+Y,3=-Y,4=+Z,5=-Z
    if (f == 0u) return vec3( 1, 0, 0);
    if (f == 1u) return vec3(-1, 0, 0);
    if (f == 2u) return vec3( 0, 1, 0);
    if (f == 3u) return vec3( 0,-1, 0);
    if (f == 4u) return vec3( 0, 0, 1);
    return vec3( 0, 0,-1);
}

vec3 unpack_rgb(uint rgba8) {
    float r = float((rgba8 >> 24) & 0xFFu) / 255.0;
    float g = float((rgba8 >> 16) & 0xFFu) / 255.0;
    float b = float((rgba8 >> 8) & 0xFFu) / 255.0;
    return vec3(r, g, b);
}

void main() {
    mat4 proj_view = camera_uniform.proj * camera_uniform.view;

    vec4 worldPos4 = pc.model * aPos;
    vFragPos = worldPos4.xyz;

    mat3 normalMat = mat3(transpose(inverse(pc.model)));
    vNormal = normalize(normalMat * face_normal(aFace));

    vColor = unpack_rgb(aColor);
    vAO = float(aColor & 0xFFu) / 255.0;

    gl_Position = proj_view * worldPos4;
}
