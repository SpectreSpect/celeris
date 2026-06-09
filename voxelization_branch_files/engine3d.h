#pragma once

#include <filesystem>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <iostream>
#include <string>
#include <chrono>

// #include "window.h"
#include "camera.h"
#include "vertex_shader.h"
#include "fragment_shader.h"
#include "vf_program.h"

#include "path_utils.h"

class Window;


class Engine3D{
public:
    Engine3D();
    ~Engine3D();

    // MeshManager* mesh_manager;
    // MaterialManager* material_manager;

    std::string default_vertex_shader_path = (executable_dir() / "shaders" / "deafult_vertex.glsl").string();
    std::string default_fragment_shader_path = (executable_dir() / "shaders" / "deafult_fragment.glsl").string();

    VertexShader* default_vertex_shader;
    FragmentShader* default_fragment_shader;
    VfProgram* default_program;

    // Camera* camera;

    int init();
    int init_glew();
    void set_window(Window* window);
    // void set_camera(Camera* camera);
    static void enable_depth_test();
    static void poll_events();
};