#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Transform {
public:
    glm::vec3 position{0.0f};
    glm::vec3 scale{1.0f,1.0f,1.0f};
    glm::vec3 rotation{0.0f,0.0f,0.0f};

    glm::mat4 local_matrix() const;
};