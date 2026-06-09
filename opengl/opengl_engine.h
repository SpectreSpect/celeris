#pragma once
#include "engine3d.h"

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

#include "mesh_manager.h"
#include "material_manager.h"
#include "path_utils.h"
#include "shader_manager.h"
#include "path_utils.h"
#include "light_source.h"
#include "ssbo.h"
#include <unordered_set>
#include "math_utils.h"
#include "compute_program.h"
#include "lighting_system.h"
#include "texture_manager.h"
#include "vulkan_window.h"

class Window;

class OpenGLEngine : public Engine3D{
public:
    OpenGLEngine();
    ~OpenGLEngine();

    VulkanWindow* window;
    
    virtual int init() override;
    virtual int init_glew();
    virtual void set_window(Window* window) override;
    virtual void set_vulkan_window(VulkanWindow* window);

    virtual void enable_depth_test() override;
    virtual  void poll_events() override;

    virtual void begin_frame(const glm::vec4& clear_color) override;
    virtual void end_frame() override;
    virtual void on_framebuffer_resized(int w, int h) override;
};