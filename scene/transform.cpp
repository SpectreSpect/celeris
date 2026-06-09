#include "transform.h"

glm::mat4 Transform::local_matrix() const {
    // Build S, R, T then compose: model = T * R * S
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);

    // R: compose rotations (order matters). Here: Rz * Ry * Rx
    glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), rotation.x, glm::vec3(1,0,0));
    glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), rotation.y, glm::vec3(0,1,0));
    glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), rotation.z, glm::vec3(0,0,1));
    glm::mat4 R  = Rz * Ry * Rx;

    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);

    return T * R * S;
}