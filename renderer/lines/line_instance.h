#pragma once
#include <glm/glm.hpp>

struct LineInstance {
    alignas(16) glm::vec3 p0;
    alignas(16) glm::vec3 p1;
    alignas(16) glm::vec4 color;
};
