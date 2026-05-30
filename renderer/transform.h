#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "../vulkan_self/logger/logger_header.h"


class Transform {
public:
    _XCLASS_NAME(Transform);

    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f,1.0f,1.0f};
    glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    Transform() = default;
    Transform(glm::vec3 position, glm::vec3 scale, glm::quat rotation);

    Transform& operator*(Transform& other);

    glm::mat4 get_model_matrix() const;
};