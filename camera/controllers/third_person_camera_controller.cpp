#include "third_person_camera_controller.h"

#include <algorithm>
#include <cmath>

#include <imgui.h>

ThirdPersonCameraController::ThirdPersonCameraController(
    Camera& camera,
    glm::vec3 target)
    : m_camera(camera), m_target(target)
{
    update_camera();
}

void ThirdPersonCameraController::set_target(glm::vec3 target) noexcept {
    m_target = target;
}

glm::vec3 ThirdPersonCameraController::target() const noexcept {
    return m_target;
}

void ThirdPersonCameraController::update_mouse(Window& window) {
    MouseState mouse_state = window.mouse_state();

    if (mouse_state.left_pressed && mouse_state.mode != MouseMode::DISABLED) {
        window.disable_cursor();
        m_first_mouse = true;
    }

    if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS &&
        mouse_state.mode != MouseMode::NORMAL) {
        window.show_cursor();
        m_first_mouse = true;
        return;
    }

    if (mouse_state.mode != MouseMode::DISABLED || !mouse_state.initialized)
        return;

    if (m_first_mouse) {
        m_last_x = static_cast<float>(mouse_state.x);
        m_last_y = static_cast<float>(mouse_state.y);
        m_first_mouse = false;
        return;
    }

    float dx = static_cast<float>(mouse_state.x) - m_last_x;
    float dy = m_last_y - static_cast<float>(mouse_state.y);

    m_last_x = static_cast<float>(mouse_state.x);
    m_last_y = static_cast<float>(mouse_state.y);

    yaw += dx * mouse_sensitivity;
    pitch += dy * mouse_sensitivity;
    pitch = std::clamp(pitch, -89.0f, 89.0f);
}

void ThirdPersonCameraController::update_zoom(
    Window& window,
    bool ui_wants_mouse)
{
    double scroll_y = window.consume_scroll_y();
    if (ui_wants_mouse || scroll_y == 0.0)
        return;

    distance -= static_cast<float>(scroll_y) * zoom_sensitivity;
    distance = std::clamp(distance, min_distance, max_distance);
}

void ThirdPersonCameraController::update_camera() {
    float yaw_radians = glm::radians(yaw);
    float pitch_radians = glm::radians(pitch);

    glm::vec3 front;
    front.x = std::cos(yaw_radians) * std::cos(pitch_radians);
    front.y = std::sin(pitch_radians);
    front.z = std::sin(yaw_radians) * std::cos(pitch_radians);

    m_camera.front = glm::normalize(front);
    m_camera.position = m_target - m_camera.front * distance;
}

void ThirdPersonCameraController::update(Window& window, float delta_time) {
    (void)delta_time;

    ImGuiIO& io = ImGui::GetIO();
    update_zoom(window, io.WantCaptureMouse);

    if (!io.WantCaptureMouse && !io.WantCaptureKeyboard)
        update_mouse(window);

    update_camera();
}
