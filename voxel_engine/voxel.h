#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Voxel{
public:
    glm::vec3 color;
    bool visible = false;
    float curvature = 0.0f;

    Voxel(glm::vec3 color = glm::vec3({1.0f, 1.0f, 1.0f}), bool visible = false, float curvature = 0.0f);
};