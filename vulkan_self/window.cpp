#include "window.h"

Window::Window(const GlfwContext&, uint32_t width, uint32_t height, std::string_view title)
    :   m_width(width), m_height(height), m_title(title) {
    LOG_METHOD();

    logger.log() << "GLFW version: " << clr(glfwGetVersionString(), "#ffaa2c") << "\n";

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        this->m_title.c_str(),
        nullptr,
        nullptr
    );

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, resize_callback);

    logger.check(m_window, "Failed to create GLFW window");
}

Window::~Window() noexcept {
    destroy();
}

void Window::destroy() noexcept {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}

Window::Window(Window&& other) noexcept
    :   m_window(std::exchange(other.m_window, nullptr)),
        m_width(std::exchange(other.m_width, 0)),
        m_height(std::exchange(other.m_height, 0)),
        m_title(std::move(other.m_title)),
        is_window_resized(std::exchange(other.is_window_resized, false)) {}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        destroy();

        m_window = std::exchange(other.m_window, nullptr);
        m_width = std::exchange(other.m_width, 0);
        m_height = std::exchange(other.m_height, 0);
        m_title = std::move(other.m_title);
        is_window_resized = std::exchange(other.is_window_resized, false);
    }

    return *this;
}

GLFWwindow* Window::handle() const { 
    return m_window; 
}

bool Window::should_close() const {
    // LOG_METHOD();
    return glfwWindowShouldClose(m_window);
}

void Window::poll_events() const {
    // LOG_METHOD();
    glfwPollEvents();
}

uint32_t Window::width() const noexcept {
    return m_width;
}

uint32_t Window::height() const noexcept {
    return m_height;
}

const std::string& Window::title() const noexcept {
    return m_title;
}

void Window::wait_until_framebuffer_available() {
    LOG_METHOD();

    int width = 0;
    int height = 0;

    glfwGetFramebufferSize(m_window, &width, &height);

    while (width == 0 || height == 0) {
        glfwWaitEvents();
        glfwGetFramebufferSize(m_window, &width, &height);
    }
}

void Window::resize_callback(GLFWwindow* handle, int width, int height) {
    LOG_NAMED("Window");

    Window* window = static_cast<Window*>(glfwGetWindowUserPointer(handle));
    window->m_width = width;
    window->m_height = height;
    window->is_window_resized = true;
}