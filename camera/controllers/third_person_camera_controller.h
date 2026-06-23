#pragma once

#include "../camera.h"
#include "../../vulkan_self/window.h"

class ThirdPersonCameraController {
public:
    explicit ThirdPersonCameraController(
        Camera& camera,
        glm::vec3 target = glm::vec3(0.0f)
    );

    void update(Window& window, float delta_time);

    void set_target(glm::vec3 target) noexcept;
    glm::vec3 target() const noexcept;

    float yaw = -90.0f;
    float pitch = -20.0f;
    float distance = 10.0f;
    float min_distance = 1.0f;
    float max_distance = 100.0f;
    float mouse_sensitivity = 0.15f;
    float zoom_sensitivity = 1.0f;

private:
    void update_mouse(Window& window);
    void update_zoom(Window& window, bool ui_wants_mouse);
    void update_camera();

    Camera& m_camera;
    glm::vec3 m_target{0.0f};

    bool m_first_mouse = true;
    float m_last_x = 0.0f;
    float m_last_y = 0.0f;
};
