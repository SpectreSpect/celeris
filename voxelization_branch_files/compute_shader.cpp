#include "compute_shader.h"

ComputeShader::ComputeShader(
    const std::filesystem::path& path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_path) 
    :   Shader(GL_COMPUTE_SHADER, path, include_directories, debug_path)
{

}