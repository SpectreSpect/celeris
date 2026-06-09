#include "voxel.h"

Voxel::Voxel(glm::vec3 color, bool visible, float curvature) {
    this->color = color;
    this->visible = visible;
    this->curvature = curvature;
}