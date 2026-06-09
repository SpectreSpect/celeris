#version 330 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec3 aColor;

out vec3 vNormal;
out vec3 vFragPos;
out vec3 vColor;

uniform mat4 uMVP;
uniform mat4 uModel;

void main() {
    vFragPos = vec3(uModel * vec4(aPos, 1.0));
    // vColor = aColor;
    vNormal = mat3(transpose(inverse(uModel))) * aNormal;
    vColor = aColor;

    gl_Position = uMVP * vec4(aPos, 1.0);
}