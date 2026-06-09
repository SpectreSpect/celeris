#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include "shader.h"

class VertexShader : public Shader {
public:
    VertexShader() = default;
    VertexShader(
        const std::filesystem::path& path, 
        const std::vector<std::filesystem::path>* include_directories = nullptr,
        const std::filesystem::path* debug_path = nullptr
    );
    
    VertexShader(const VertexShader&) = delete;
    VertexShader& operator=(const VertexShader&) = delete;
    VertexShader(VertexShader&& o) noexcept;
    VertexShader& operator=(VertexShader&& o) noexcept;
};