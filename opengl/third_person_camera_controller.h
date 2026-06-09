#pragma once
#include "camera.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "window.h"

class ThirdPersonController {
public:
    Camera* camera = nullptr;

    bool first_mouse = true;
    float last_x = 0.0f;
    float last_y = 0.0f;

    float yaw = -90.0f;
    float pitch = 20.0f;
    float mouse_sensitivity = 0.15f;

    float distance = 5.0f;
    float min_distance = 1.0f;
    float max_distance = 20.0f;

    glm::vec3 target_position = glm::vec3(0.0f);
    glm::vec3 look_offset = glm::vec3(0.0f, 1.7f, 0.0f);
    glm::vec3 world_up = glm::vec3(0.0f, 1.0f, 0.0f);

    // --- stored input state ---
    bool forward_pressed = false;
    bool backward_pressed = false;
    bool left_pressed = false;
    bool right_pressed = false;
    bool jump_pressed = false;
    bool run_pressed = false;

    // Raw movement input in local 2D form:
    // x: left/right, y: forward/back
    glm::vec2 move_input = glm::vec2(0.0f);

    // Final world-space direction relative to camera
    glm::vec3 move_direction = glm::vec3(0.0f);

    ThirdPersonController(Camera* camera);

    void update_keyboard(Window* window, float delta_time);
    void update_mouse(Window* window, float delta_time);
    void update_camera();
    void update(Window* window, float delta_time);

    glm::vec3 get_look_target() const;
    glm::vec3 get_flat_forward() const;
    glm::vec3 get_flat_right() const;

    void update_movement_direction();
};