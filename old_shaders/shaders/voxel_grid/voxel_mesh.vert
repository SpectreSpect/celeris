#version 330 core

layout(location = 0) in vec4 aPos;      // из vb[].pos (vec4)
layout(location = 1) in uint aColor;    // из vb[].color (RGBA8)
layout(location = 2) in uint aFace;     // из vb[].face (0..5)

uniform mat4 uWorld;
uniform mat4 uProjView;

out vec3 vNormal;
out vec3 vFragPos;
out vec3 vColor;
out float vAO;

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
    vec4 worldPos4 = uWorld * aPos;
    vFragPos = worldPos4.xyz;

    mat3 normalMat = mat3(transpose(inverse(uWorld)));
    vNormal = normalize(normalMat * face_normal(aFace));

    vColor = unpack_rgb(aColor);
    vAO = float(aColor & 0xFFu) / 255.0;

    gl_Position = uProjView * worldPos4;
}
