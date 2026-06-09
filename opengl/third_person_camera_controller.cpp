#include "third_person_camera_controller.h"
#include "imgui_layer.h"

ThirdPersonController::ThirdPersonController(Camera* camera) {
    this->camera = camera;
    update_camera();
}

glm::vec3 ThirdPersonController::get_look_target() const {
    return target_position + look_offset;
}

glm::vec3 ThirdPersonController::get_flat_forward() const {
    glm::vec3 f = camera->front;
    f.y = 0.0f;

    float len = glm::length(f);
    if (len < 1e-6f) {
        f.x = cos(glm::radians(yaw));
        f.y = 0.0f;
        f.z = sin(glm::radians(yaw));
        len = glm::length(f);

        if (len < 1e-6f) {
            return glm::vec3(0.0f, 0.0f, -1.0f);
        }
    }

    return f / len;
}

glm::vec3 ThirdPersonController::get_flat_right() const {
    return glm::normalize(glm::cross(get_flat_forward(), world_up));
}

void ThirdPersonController::update_movement_direction() {
    move_input = glm::vec2(0.0f);

    if (right_pressed)    move_input.x += 1.0f;
    if (left_pressed)     move_input.x -= 1.0f;
    if (forward_pressed)  move_input.y += 1.0f;
    if (backward_pressed) move_input.y -= 1.0f;

    glm::vec3 forward = get_flat_forward();
    glm::vec3 right   = get_flat_right();

    move_direction = forward * move_input.y + right * move_input.x;

    float len = glm::length(move_direction);
    if (len > 1e-6f) {
        move_direction /= len;
    } else {
        move_direction = glm::vec3(0.0f);
    }
}

void ThirdPersonController::update_keyboard(Window* window, float delta_time) {
    if (window->mouse_state.mode != MouseMode::DISABLED)
        return;

    forward_pressed  = glfwGetKey(window->window, GLFW_KEY_W) == GLFW_PRESS;
    backward_pressed = glfwGetKey(window->window, GLFW_KEY_S) == GLFW_PRESS;
    left_pressed     = glfwGetKey(window->window, GLFW_KEY_A) == GLFW_PRESS;
    right_pressed    = glfwGetKey(window->window, GLFW_KEY_D) == GLFW_PRESS;
    jump_pressed     = glfwGetKey(window->window, GLFW_KEY_SPACE) == GLFW_PRESS;
    run_pressed      = glfwGetKey(window->window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

    update_movement_direction();

    if (glfwGetKey(window->window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (window->mouse_state.mode != MouseMode::NORMAL)
            window->show_cursor();
    }
}

void ThirdPersonController::update_mouse(Window* window, float delta_time) {
    MouseState mouse_state = window->mouse_state;

    if (mouse_state.left_pressed) {
        if (mouse_state.mode != MouseMode::DISABLED) {
            window->disable_cursor();
            first_mouse = true;
        }
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

    float xoffset = dx * mouse_sensitivity;
    float yoffset = dy * mouse_sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}

void ThirdPersonController::update_camera() {
    glm::vec3 look_target = get_look_target();

    glm::vec3 look_dir;
    look_dir.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    look_dir.y = sin(glm::radians(pitch));
    look_dir.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    look_dir = glm::normalize(look_dir);

    distance = glm::clamp(distance, min_distance, max_distance);

    camera->position = look_target - look_dir * distance;
    camera->front = glm::normalize(look_target - camera->position);
    camera->up = world_up;
}

void ThirdPersonController::update(Window* window, float delta_time) {
    ImGuiIO& io = ImGui::GetIO();

    bool ui_wants_mouse = io.WantCaptureMouse;
    bool ui_wants_keys  = io.WantCaptureKeyboard;

    if (!ui_wants_mouse && !ui_wants_keys) {
        update_keyboard(window, delta_time);
        update_mouse(window, delta_time);
    }

    update_camera();
}