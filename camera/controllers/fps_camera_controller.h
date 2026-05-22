#pragma once
#include "../camera.h"

// #include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "../../vulkan_self/window.h"

class FPSCameraController {
public:
    bool first_mouse = true;
    float last_x = 0.0f;
    float last_y = 0.0f;

    float yaw = -90.0f;
    float pitch = 0.0f;
    float mouse_sensitivity = 0.15f;
    float speed = 5.0f;

    FPSCameraController(Camera& camera);

    void update_keyboard(Window& window, float delta_time);
    void update_mouse(Window& window, float delta_time);
    void update(Window& window, float delta_time);

    void move_forward(float dt) {
        m_camera.position += m_camera.front * speed * dt;
    }

    void move_backward(float dt) {
        m_camera.position -= m_camera.front * speed * dt;
    }

    void move_right(float dt) {
        glm::vec3 right = glm::normalize(glm::cross(m_camera.front, m_camera.up));
        m_camera.position += right * speed * dt;
    }

    void move_left(float dt) {
        glm::vec3 right = glm::normalize(glm::cross(m_camera.front, m_camera.up));
        m_camera.position -= right * speed * dt;
    }

    void move_up(float dt) {
        m_camera.position += m_camera.up * speed * dt;
    }

    void move_down(float dt) {
        m_camera.position -= m_camera.up * speed * dt;
    }

private:
    Camera& m_camera;
};