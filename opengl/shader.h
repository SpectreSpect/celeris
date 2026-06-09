#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>

#include "glsl_preprocessor.h"

class Shader {
public:
    GLuint id;

    Shader() = default;
    Shader(
        GLenum shader_type,
        const std::filesystem::path& path, 
        const std::vector<std::filesystem::path>* include_directories = nullptr,
        const std::filesystem::path* debug_path = nullptr
    );
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&& o) noexcept { id = o.id; o.id = 0; }
    Shader& operator=(Shader&& o) noexcept {
        if (this != &o) {
            if (id) glDeleteShader(id);
            id = o.id;
            o.id = 0;
        }
        return *this;
    }
    void print_shader_log(const char* name = "NO NAME");

    template<class T>
    static inline T build_shader(
        const std::filesystem::path& relative_path, 
        const std::filesystem::path& root_path,
        const std::filesystem::path* debug_root_path,
        const std::vector<std::filesystem::path>* include_directories = nullptr)
    {
        std::filesystem::path path = root_path / relative_path;
        std::filesystem::path debug_path;

        if (debug_root_path != nullptr) {
            debug_path = *debug_root_path / relative_path;
        } 

        return T(path, include_directories, debug_root_path != nullptr ? &debug_path : nullptr);
    }

    static std::string load_text_file(
        const std::filesystem::path& path, 
        const std::vector<std::filesystem::path>* include_directories = nullptr
    );
    static GLuint compile_shader(GLenum type, const char* src, const std::filesystem::path* shader_path = nullptr);
};