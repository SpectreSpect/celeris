#include "glfw_context.h"

GlfwContext::GlfwContext() {
    glfwSetErrorCallback(glfw_error_callback);
    logger.check(glfwInit(), "Failed to initialize GLFW");
}

GlfwContext::~GlfwContext() {
    glfwTerminate();
}

void GlfwContext::glfw_error_callback(int code, const char* description) {
    LOG_NAMED("GlfwContext");

    logger.log_traceback();

    std::cerr << "GLFW error [" << code << "]: " << description << std::endl;

    MultiColorString header = {
        {"GLFW error [", LoggerPalette::error},
        {std::to_string(code), LoggerPalette::yellow},
        {"]: ", LoggerPalette::error}
    };

    std::cerr << header << clr(description, LoggerPalette::error) << std::endl;
}