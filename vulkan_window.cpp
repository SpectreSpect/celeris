#include "vulkan_window.h"
#include "vulkan_engine.h"
#include <iostream>

VulkanWindow::VulkanWindow(VulkanEngine* engine, int width, int height, const std::string& title) {
    this->engine = engine;
    this->width = width;
    this->height = height;

    this->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!this->window) {
        std::cerr << "glfwCreateWindow failed\n";
        return;
    }

    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, VulkanWindow::mouse_callback);
    glfwSetMouseButtonCallback(window, VulkanWindow::mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, VulkanWindow::framebuffer_size_callback);
}

VulkanWindow::~VulkanWindow() {
    if (window) {
        glfwDestroyWindow(window);
    }
}

bool VulkanWindow::is_open() const {
    return window && !glfwWindowShouldClose(window);
}

float VulkanWindow::get_aspect_ratio() const {
    return height == 0 ? 1.0f : float(width) / float(height);
}

float VulkanWindow::get_fbuffer_aspect_ratio() const {
    int fbWidth = 1, fbHeight = 1;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    if (fbHeight == 0) fbHeight = 1;
    return float(fbWidth) / float(fbHeight);
}

void VulkanWindow::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    VulkanWindow* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
    if (!self) return;

    self->width = width;
    self->height = height;
    self->framebuffer_resized = true;
}

void VulkanWindow::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    VulkanWindow* win = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
    if (!win) return;

    win->mouse_state.x = xpos;
    win->mouse_state.y = ypos;
    win->mouse_state.initialized = true;
}

void VulkanWindow::mouse_button_callback(GLFWwindow* win, int button, int action, int mods) {
    VulkanWindow* self = static_cast<VulkanWindow*>(glfwGetWindowUserPointer(win));
    if (!self) return;

    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:   self->mouse_state.left_pressed   = (action == GLFW_PRESS); break;
        case GLFW_MOUSE_BUTTON_RIGHT:  self->mouse_state.right_pressed  = (action == GLFW_PRESS); break;
        case GLFW_MOUSE_BUTTON_MIDDLE: self->mouse_state.middle_pressed = (action == GLFW_PRESS); break;
        default: break;
    }
}

void VulkanWindow::set_camera(Camera* camera) {
    this->camera = camera;
}

void VulkanWindow::disable_cursor() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    mouse_state.mode = MouseMode::DISABLED;
}

void VulkanWindow::hide_cursor() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    mouse_state.mode = MouseMode::HIDDEN;
}

void VulkanWindow::show_cursor() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    mouse_state.mode = MouseMode::NORMAL;
}