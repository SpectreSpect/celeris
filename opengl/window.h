#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "drawable.h"
#include "camera.h"
#include "mouse_state.h"
#include "drawable.h"
// #include "engine3d.h"

class Engine3D;
class Program;
class Line;

class Window{
public:
    Engine3D* engine;
    GLFWwindow* window;
    int width;
    int height;
    MouseState mouse_state;

    // double mouse_dx = 0;
    // double mouse_dy = 0;

    // float last_mouse_x = 0;
    // float last_mouse_y = 0;



    Camera* camera;

    Window(Engine3D* engine, int width, int height, std::string title);
    ~Window();
    void swap_buffers();
    bool is_open();
    float get_aspect_ratio();
    float get_fbuffer_aspect_ratio() const;

    static void framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
    static void mouse_button_callback(GLFWwindow* win, int button, int action, int mods) ;

    void set_camera(Camera* camera);
    void disable_cursor();
    void hide_cursor();
    void show_cursor();
    
    // void draw(Drawable* drawable, Camera* camera, Program* program = nullptr);


    void clear_color(const glm::vec4& color);
};