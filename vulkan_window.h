#pragma once

#include <GLFW/glfw3.h>
#include <string>
#include "mouse_state.h"

class Camera;
class VulkanEngine;

class VulkanWindow {
public:
    VulkanEngine* engine = nullptr;
    GLFWwindow* window = nullptr;
    int width = 0;
    int height = 0;
    bool framebuffer_resized = false;
    MouseState mouse_state;
    Camera* camera = nullptr;

    VulkanWindow(VulkanEngine* engine, int width, int height, const std::string& title);
    ~VulkanWindow();

    bool is_open() const;
    float get_aspect_ratio() const;
    float get_fbuffer_aspect_ratio() const;

    void set_camera(Camera* camera);
    void disable_cursor();
    void hide_cursor();
    void show_cursor();

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* win, int button, int action, int mods);
};