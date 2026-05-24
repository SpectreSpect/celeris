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

struct PointUniform {
    float point_size_px;
    float point_size_world;
    int screen_space_size;
    float pad;
};