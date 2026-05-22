 #include "transform.h"
 
Transform::Transform(glm::vec3 position, glm::vec3 scale, glm::vec3 rotation)
    :   position(position),
        scale(scale),
        rotation(rotation){}

glm::mat4 Transform::get_model_matrix() const {
    LOG_METHOD();

    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 R = glm::mat4_cast(glm::normalize(rotation));
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    return T * R * S;
}
