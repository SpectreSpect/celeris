#pragma once

#include "../renderer/lines/line_cloud.h"
#include "../renderer/scene_object.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

class MaterialInstanceManager;
class MeshManager;
class VulkanEngine;

class Arrow : public SceneObject {
public:
    _XCHILD_NAME(Arrow);

    Arrow(
        VulkanEngine& engine,
        MeshManager& mesh_manager,
        MaterialInstanceManager& material_instance_manager,
        glm::vec4 color = glm::vec4(1.0f),
        float length = 1.0f,
        float width = 3.0f,
        glm::vec3 position = glm::vec3(0.0f),
        glm::vec3 direction = glm::vec3(1.0f, 0.0f, 0.0f),
        float wing_length = 0.25f,
        float wing_angle = glm::radians(30.0f)
    );

    void set_color(glm::vec4 color);
    void set_length(float length);
    void set_width(float width);
    void set_position(glm::vec3 position);
    void set_direction(glm::vec3 direction);
    void set_wing_length(float wing_length);
    void set_wing_angle(float wing_angle);

    glm::vec4 color() const noexcept;
    float length() const noexcept;
    float width() const noexcept;
    glm::vec3 position() const noexcept;
    glm::vec3 direction() const noexcept;
    float wing_length() const noexcept;
    float wing_angle() const noexcept;

private:
    LineCloud m_line_cloud;
    glm::vec4 m_color{1.0f};
    float m_length = 1.0f;
    float m_width = 3.0f;
    glm::vec3 m_direction{1.0f, 0.0f, 0.0f};
    float m_wing_length = 0.25f;
    float m_wing_angle = glm::radians(30.0f);

    void refresh_lines();
    void refresh_material();
};
