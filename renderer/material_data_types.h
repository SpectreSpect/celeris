#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

struct UnlitMaterialData {
    glm::vec4 color;
};

struct BlinPhongMaterialData {
    glm::vec4 material;
    glm::vec4 color;
};

struct SkyboxMaterialData {
    float exposure;
};

struct PBRMaterialData {
    glm::vec4 material = glm::vec4(1.0f, 0.01f, 1.0f, 0.2f);
    glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
};

struct PointUniform {
    float point_size_px;
    float point_size_world;
    int screen_space_size;
    float pad;
};