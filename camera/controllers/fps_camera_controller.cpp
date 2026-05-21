#include "fps_camera_controller.h"


FPSCameraController::FPSCameraController(Camera& camera) : m_camera(camera) {}

void FPSCameraController::update_keyboard(Window& window, float delta_time) {
    if (window.mouse_state().mode != MouseMode::DISABLED)
        return;

    glm::vec3 right = glm::normalize(glm::cross(m_camera.front, m_camera.up));
    if (glfwGetKey(window.handle(), GLFW_KEY_W) == GLFW_PRESS) move_forward(delta_time);
    if (glfwGetKey(window.handle(), GLFW_KEY_S) == GLFW_PRESS) move_backward(delta_time);
    if (glfwGetKey(window.handle(), GLFW_KEY_A) == GLFW_PRESS) move_left(delta_time);
    if (glfwGetKey(window.handle(), GLFW_KEY_D) == GLFW_PRESS) move_right(delta_time);
    if (glfwGetKey(window.handle(), GLFW_KEY_SPACE) == GLFW_PRESS) move_up(delta_time);
    if (glfwGetKey(window.handle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) move_down(delta_time);

    if (glfwGetKey(window.handle(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        if (window.mouse_state().mode != MouseMode::NORMAL)
            window.show_cursor();
}

void FPSCameraController::update_mouse(Window& window, float delta_time) {
    MouseState mouse_state = window.mouse_state();


    if (mouse_state.left_pressed)
        if (mouse_state.mode != MouseMode::DISABLED) {
            window.disable_cursor();
            first_mouse = true;
        }
    
    if (mouse_state.mode != MouseMode::DISABLED)
        return;

    if (!mouse_state.initialized)
        return;
    
    if (first_mouse) {
        last_x = mouse_state.x;
        last_y = mouse_state.y;
        first_mouse = false;
    }
        
    float dx = mouse_state.x - last_x;
    float dy = last_y - mouse_state.y;

    if (dx == 0.0f && dy == 0.0f) 
        return;

    last_x = mouse_state.x; 
    last_y = mouse_state.y;
    
    double xoffset = dx * mouse_sensitivity;
    double yoffset = dy * mouse_sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    m_camera.front = glm::normalize(front);
}

void FPSCameraController::update(Window& window, float delta_time) {
    // ImGuiIO& io = ImGui::GetIO();

    // bool ui_wants_mouse = io.WantCaptureMouse;
    // bool ui_wants_keys  = io.WantCaptureKeyboard;

    // if (!ui_wants_mouse && !ui_wants_keys) {
    update_keyboard(window, delta_time);
    update_mouse(window, delta_time);
    // }
}