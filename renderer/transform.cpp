#include "transform.h"

#include <cmath>
#include <stdexcept>

Transform::Transform(glm::vec3 position, glm::vec3 scale, glm::quat rotation)
    :   position(position),
        scale(scale),
        rotation(rotation){}

Transform Transform::from_matrix(const glm::mat4& matrix) {
    constexpr float epsilon = 1e-5f;

    for (glm::length_t column = 0; column < 4; ++column) {
        for (glm::length_t row = 0; row < 4; ++row) {
            if (!std::isfinite(matrix[column][row])) {
                throw std::domain_error("Cannot decompose a matrix with non-finite values");
            }
        }
    }

    const bool is_affine =
        glm::abs(matrix[0][3]) <= epsilon &&
        glm::abs(matrix[1][3]) <= epsilon &&
        glm::abs(matrix[2][3]) <= epsilon &&
        glm::abs(matrix[3][3] - 1.0f) <= epsilon;

    if (!is_affine) {
        throw std::domain_error("Only affine matrices can be represented by Transform");
    }

    glm::vec3 axis_x(matrix[0]);
    glm::vec3 axis_y(matrix[1]);
    glm::vec3 axis_z(matrix[2]);

    glm::vec3 result_scale(
        glm::length(axis_x),
        glm::length(axis_y),
        glm::length(axis_z)
    );

    if (glm::any(glm::lessThanEqual(result_scale, glm::vec3(epsilon)))) {
        throw std::domain_error(
            "A matrix with a zero or near-zero scale cannot be uniquely decomposed"
        );
    }

    axis_x /= result_scale.x;
    axis_y /= result_scale.y;
    axis_z /= result_scale.z;

    const bool has_shear =
        glm::abs(glm::dot(axis_x, axis_y)) > epsilon ||
        glm::abs(glm::dot(axis_x, axis_z)) > epsilon ||
        glm::abs(glm::dot(axis_y, axis_z)) > epsilon;

    if (has_shear) {
        throw std::domain_error("A matrix with shear cannot be represented by Transform");
    }

    glm::mat3 result_rotation(axis_x, axis_y, axis_z);
    if (glm::determinant(result_rotation) < 0.0f) {
        axis_x = -axis_x;
        result_scale.x = -result_scale.x;
        result_rotation = glm::mat3(axis_x, axis_y, axis_z);
    }

    Transform result;
    result.position = glm::vec3(matrix[3]);
    result.rotation = glm::normalize(glm::quat_cast(result_rotation));
    result.scale = result_scale;
    return result;
}

glm::mat4 Transform::get_model_matrix() const {
    LOG_METHOD();

    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 R = glm::mat4_cast(glm::normalize(rotation));
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    return T * R * S;
}

Transform Transform::operator*(const Transform& other) const {
    Transform result;
    result.position = position + rotation * (scale * other.position);
    result.rotation = glm::normalize(rotation * other.rotation);
    result.scale = scale * other.scale;
    return result;
}

Transform Transform::inverse() const {
    return from_matrix(get_inverse_model_matrix());
}

glm::mat4 Transform::get_inverse_model_matrix() const {
    constexpr float epsilon = 1e-5f;
    if (glm::any(glm::lessThanEqual(glm::abs(scale), glm::vec3(epsilon)))) {
        throw std::domain_error(
            "A transform with a zero or near-zero scale component is not invertible"
        );
    }

    return glm::inverse(get_model_matrix());
}
