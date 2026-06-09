#include "vertex_shader.h"


VertexShader::VertexShader(
    const std::filesystem::path& path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_path) 
    :   Shader(GL_VERTEX_SHADER, path, include_directories, debug_path)   
{

}

VertexShader::VertexShader(VertexShader&& o) noexcept {
    id = o.id;
    o.id = 0;
}

VertexShader& VertexShader::operator=(VertexShader&& o) noexcept {
    if (this != &o) {
        if (id) glDeleteShader(id);
        id = o.id;
        o.id = 0;
    }
    return *this;
}