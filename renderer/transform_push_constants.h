#pragma once

#include <glm/glm.hpp>

struct TransformPushConstants {
    glm::mat4 model;
    uint32_t material_data_id;
};
