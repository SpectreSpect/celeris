# include "camera.h"


Camera::Camera(glm::vec3 pos, glm::vec3 up_vec, float fov_deg)
{
    this->position = pos;
    this->up = up_vec;
    this->fov = fov_deg;
    // front = {0.0f, 0.0f, 1.0f};
    front = {0.0f, 0.0f, -1.0f};

    // create_clusters(clusters, num_clusters, glm::radians(this->fov), 1280.0f / 720.0f, near, far);
}

glm::mat4 Camera::get_view_matrix() const {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::get_projection_matrix(float aspect_ratio) const {
    glm::mat4 proj = glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
    return proj;
}