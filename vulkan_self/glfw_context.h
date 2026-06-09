#pragma once

#include <string>
#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class GlfwContext {
public:
    GlfwContext();
    ~GlfwContext();

    GlfwContext(const GlfwContext&) = delete;
    GlfwContext& operator=(const GlfwContext&) = delete;

    GlfwContext(GlfwContext&&) noexcept = delete;
    GlfwContext& operator=(GlfwContext&&) noexcept = delete;

    static void glfw_error_callback(int code, const char* description);
};
