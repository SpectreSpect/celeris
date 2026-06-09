#include "shader.h"

Shader::Shader(
    GLenum shader_type, 
    const std::filesystem::path& path, 
    const std::vector<std::filesystem::path>* include_directories,
    const std::filesystem::path* debug_path)
{
    std::string src = load_text_file(path, include_directories);

    if (debug_path != nullptr) {
        std::filesystem::create_directories(debug_path->parent_path());

        std::ofstream debug_out(*debug_path);
        debug_out << src;
        debug_out.close();
    }

    this->id = compile_shader(shader_type, src.c_str(), &path);
}

Shader::~Shader() {
    glDeleteShader(id);
}

void Shader::print_shader_log(const char* name) {
    GLint ok = 0;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);

    GLint len = 0;
    glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);

    std::string log;
    if (len > 1) {
        log.resize(len);
        glGetShaderInfoLog(id, len, nullptr, log.data());
    }

    std::cout << "[COMPILE] " << name
              << " shader=" << id
              << " ok=" << ok << "\n";
    if (!log.empty()) std::cout << log << "\n";
}

std::string Shader::load_text_file(
    const std::filesystem::path& path, 
    const std::vector<std::filesystem::path>* include_directories) 
{
    GlslPreprocessor preprocessor;
    return preprocessor.load(path, include_directories);
}

GLuint Shader::compile_shader(GLenum type, const char* src, const std::filesystem::path* shader_path) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char buf[1024];
        glGetShaderInfoLog(s, sizeof(buf), nullptr, buf);
        
        if (shader_path != nullptr) {
            std::filesystem::path shader_name = shader_path->filename();
            std::cout << "Error in shader " << shader_name << ":" << std::endl;
            std::cout << buf << std::endl;
        } else {
            std::cerr << "Shader compile error: " << buf << std::endl;
        }
    }
    return s;
}
