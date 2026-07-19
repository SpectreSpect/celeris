#include "arrow.h"

#include "../managers/material_instance_manager.h"
#include "../managers/mesh_manager.h"
#include "../renderer/lines/line_instance.h"
#include "../renderer/material_data_types.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

Arrow::Arrow(
    VulkanEngine& engine,
    MeshManager& mesh_manager,
    MaterialInstanceManager& material_instance_manager,
    glm::vec4 color,
    float length,
    float width,
    glm::vec3 position,
    glm::vec3 direction,
    float wing_length,
    float wing_angle)
    : m_line_cloud(engine, mesh_manager.line_quad, material_instance_manager.line, 3),
      m_color(color),
      m_length(std::max(0.0f, length)),
      m_width(std::max(0.0f, width)),
      m_wing_length(std::max(0.0f, wing_length)),
      m_wing_angle(glm::clamp(wing_angle, 0.0f, glm::pi<float>())) {
    refresh_material();

    add_child(m_line_cloud);
    set_position(position);
    set_direction(direction);
}

void Arrow::set_color(glm::vec4 color) {
    m_color = color;
    refresh_lines();
}

void Arrow::set_length(float length) {
    m_length = std::max(0.0f, length);
    refresh_lines();
}

void Arrow::set_width(float width) {
    m_width = std::max(0.0f, width);
    refresh_material();
}

void Arrow::set_position(glm::vec3 position) {
    transform.position = position;
}

void Arrow::set_direction(glm::vec3 direction) {
    const float length_squared = glm::dot(direction, direction);
    if (std::isfinite(length_squared) && length_squared > 1e-12f) {
        m_direction = direction / std::sqrt(length_squared);
    }
    refresh_lines();
}

void Arrow::set_wing_length(float wing_length) {
    m_wing_length = std::max(0.0f, wing_length);
    refresh_lines();
}

void Arrow::set_wing_angle(float wing_angle) {
    m_wing_angle = glm::clamp(wing_angle, 0.0f, glm::pi<float>());
    refresh_lines();
}

glm::vec4 Arrow::color() const noexcept {
    return m_color;
}

float Arrow::length() const noexcept {
    return m_length;
}

float Arrow::width() const noexcept {
    return m_width;
}

glm::vec3 Arrow::position() const noexcept {
    return transform.position;
}

glm::vec3 Arrow::direction() const noexcept {
    return m_direction;
}

float Arrow::wing_length() const noexcept {
    return m_wing_length;
}

float Arrow::wing_angle() const noexcept {
    return m_wing_angle;
}

void Arrow::refresh_lines() {
    const glm::vec3 origin(0.0f);
    const glm::vec3 tip = m_direction * m_length;

    const glm::vec3 reference_axis = std::abs(m_direction.y) < 0.9f
        ? glm::vec3(0.0f, 1.0f, 0.0f)
        : glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 side = glm::normalize(glm::cross(m_direction, reference_axis));
    const glm::vec3 wing_back = -m_direction * std::cos(m_wing_angle);
    const glm::vec3 wing_side = side * std::sin(m_wing_angle);
    const glm::vec3 wing_0 = tip + (wing_back + wing_side) * m_wing_length;
    const glm::vec3 wing_1 = tip + (wing_back - wing_side) * m_wing_length;

    m_line_cloud.set_lines(std::vector<LineInstance>{
        LineInstance{.p0 = origin, .p1 = tip, .color = m_color},
        LineInstance{.p0 = tip, .p1 = wing_0, .color = m_color},
        LineInstance{.p0 = tip, .p1 = wing_1, .color = m_color}
    });
}

void Arrow::refresh_material() {
    m_line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(1.0f),
        .line_width_pixels = m_width
    });
}
