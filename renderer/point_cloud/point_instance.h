#pragma once

#include <glm/vec4.hpp>

struct alignas(16) PointInstance {
    glm::vec4 position;
    glm::vec4 color;
};