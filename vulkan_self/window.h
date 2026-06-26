#pragma once
#include <string>
#include <cstdint>
#include <utility>
#include <iostream>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "glfw_context.h"
#include "mouse_state.h"

class Window {
public:
    _XCLASS_NAME(Window);

    bool is_window_resized = false;

    explicit Window() = delete;
    explicit Window(const GlfwContext&, uint32_t width, uint32_t height, std::string_view title);
    ~Window() noexcept;
    void destroy() noexcept;

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    GLFWwindow* handle() const;

    bool should_close() const;
    void poll_events() const;

    void disable_cursor();
    void hide_cursor();
    void show_cursor();

    uint32_t width() const noexcept;
    uint32_t height() const noexcept;
    const std::string& title() const noexcept;
    MouseState mouse_state() const noexcept;

    void wait_until_framebuffer_available();

private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::string m_title;

    MouseState m_mouse_state;

    GLFWwindow* m_window = nullptr;

private:
    static void resize_callback(GLFWwindow* handle, int width, int height);
    static void mouse_callback(GLFWwindow* handle, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* handle, int button, int action, int mods);
};
