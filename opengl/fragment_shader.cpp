#include "fragment_shader.h"

FragmentShader::FragmentShader(
    const std::filesystem::path& path,
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_path)
    : Shader(GL_FRAGMENT_SHADER, path, include_directories, debug_path)
{

}

FragmentShader::FragmentShader(FragmentShader&& o) noexcept {
    id = o.id; 
    o.id = 0;
}

FragmentShader& FragmentShader::operator=(FragmentShader&& o) noexcept {
    if (this != &o) {
        if (id) glDeleteShader(id);
        id = o.id;
        o.id = 0;
    }
    return *this;
}