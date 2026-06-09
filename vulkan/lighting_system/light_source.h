#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct alignas(16) LightSource {
    glm::vec4 position; // (pos_x, pos_y, pos_z, radius)
    glm::vec4 color; // (color_r, color_g, color_b, intensity)
};