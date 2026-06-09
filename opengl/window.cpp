#include "window.h"
#include "engine3d.h"
#include "line.h"

Window::Window(Engine3D* engine, int width, int height, std::string title) {
    this->engine = engine;
    this->window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    this->width = width;
    this->height = height;
    if (!this->window) {
        std::cerr << "glfwCreateWindow failed\n";
        return;
    }
    glfwSetWindowUserPointer(window, this);

    glfwSetCursorPosCallback(window, Window::mouse_callback);
    glfwSetMouseButtonCallback(window, Window::mouse_button_callback);
    glfwSetFramebufferSizeCallback(window, Window::framebuffer_size_callback);
}

Window::~Window() {
    glfwDestroyWindow(window);
}


void Window::swap_buffers() {
    glfwSwapBuffers(window);
}

bool Window::is_open(){
    return !glfwWindowShouldClose(window);
}

float Window::get_aspect_ratio() {
    if (height == 0) return 1.0f;
    return (float)width / (float)height;
}

float Window::get_fbuffer_aspect_ratio() const {
    int fbWidth, fbHeight;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    
    if (fbHeight == 0) 
        fbHeight = 1;

    return (float)fbWidth / (float)fbHeight;
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);

    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(window));
    if (!self) return;

    self->width = width;
    self->height = height;
}

void Window::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    Window* win = (Window*)glfwGetWindowUserPointer(window);

    win->mouse_state.x = xpos;
    win->mouse_state.y = ypos;
    win->mouse_state.initialized = true;
    
    // win->mouse_dx = (float)xpos - win->last_mouse_x;
    // win->mouse_dy = win->last_mouse_y - (float)ypos;

    // win->last_mouse_x = (float)xpos;
    // win->last_mouse_y = (float)ypos;

    // Camera* camera = win->camera;

    // if (!camera)
    //     return;

    // if (camera->firstMouse) {
    //     win->last_mouse_x = (float)xpos;
    //     win->last_mouse_y = (float)ypos;
    //     camera->firstMouse = false;
    // }

    // float xoffset = (float)xpos - win->last_mouse_x;
    // float yoffset = win->last_mouse_y - (float)ypos; // reversed
    // win->last_mouse_x = (float)xpos;
    // win->last_mouse_y = (float)ypos;

    // camera->processMouseMovement(xoffset, yoffset);
}

void Window::mouse_button_callback(GLFWwindow* win, int button, int action, int mods) {
    Window* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!self) return;

    

    // Update button state inside mouse_state
    switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT:
            self->mouse_state.left_pressed = (action == GLFW_PRESS);
            break;
        case GLFW_MOUSE_BUTTON_RIGHT:
            self->mouse_state.right_pressed = (action == GLFW_PRESS);
            break;
        case GLFW_MOUSE_BUTTON_MIDDLE:
            self->mouse_state.middle_pressed = (action == GLFW_PRESS);
            break;
        default:
            break;
    }
}

void Window::set_camera(Camera* camera) {
    this->camera = camera;
}

void Window::disable_cursor() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    mouse_state.mode = MouseMode::DISABLED;
}

void Window::hide_cursor() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    mouse_state.mode = MouseMode::HIDDEN;
}

void Window::show_cursor() {
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    mouse_state.mode = MouseMode::NORMAL;
}

void Window::clear_color(const glm::vec4& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// void Window::draw(Drawable* drawable, Camera* camera, Program* program) {
//     float aspect = get_fbuffer_aspect_ratio();

//     RenderState states;
//     states.proj = camera->get_projection_matrix(aspect);
//     states.vp = states.proj * camera->get_view_matrix();
//     states.transform = glm::mat4(1.0f);
//     states.program = program ? program : engine->default_program;
//     states.camera = camera;
//     states.camera->update_frustum_planes(states.vp);
//     states.engine = engine;

//     int fbW, fbH;
//     glfwGetFramebufferSize(window, &fbW, &fbH);

//     states.viewport_px = {fbW, fbH};

//     drawable->draw(states);

//     // glm::mat4 projection = camera->get_projection_matrix(aspect);
//     // glm::mat4 view = camera->get_view_matrix();
//     // glm::mat4 model = ???
//     // glm::mat4 mvp = projection * view * model;

//     // program->use();
//     // program->set_mat4("uMVP", mvp);

//     // mesh->draw();
// }