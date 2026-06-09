#pragma once
#include "shader.h"
#include <fstream>

class ComputeShader : public Shader {
public:
    ComputeShader() = default;
    ComputeShader(
        const std::filesystem::path& path, 
<<<<<<< HEAD:compute_shader.h
        const std::vector<std::filesystem::path>* include_directories = nullptr,
        const std::filesystem::path* debug_path = nullptr
=======
        const std::vector<std::filesystem::path>& include_directories = std::vector<std::filesystem::path>(),
        const std::filesystem::path& debug_path = "" 
>>>>>>> origin/cloud-to-mesh:opengl/compute_shader.h
    );

    ComputeShader(const ComputeShader&) = delete;
    ComputeShader& operator=(const ComputeShader&) = delete;
    ComputeShader(ComputeShader&& o) noexcept { id = o.id; o.id = 0; }
    ComputeShader& operator=(ComputeShader&& o) noexcept {
        if (this != &o) {
            if (id) glDeleteShader(id);
            id = o.id;
            o.id = 0;
        }
        return *this;
    }

};