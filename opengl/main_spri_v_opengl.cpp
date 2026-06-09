#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifndef GL_SHADER_BINARY_FORMAT_SPIR_V
#define GL_SHADER_BINARY_FORMAT_SPIR_V 0x9551
#endif

struct TestOutputCPU {
    double out_val[4];
};

static std::vector<char> read_binary_file(const char* path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("Failed to open file: ") + path);
    }

    std::streamsize size = file.tellg();
    if (size <= 0) {
        throw std::runtime_error(std::string("File is empty: ") + path);
    }

    file.seekg(0, std::ios::beg);
    std::vector<char> data(static_cast<size_t>(size));
    if (!file.read(data.data(), size)) {
        throw std::runtime_error(std::string("Failed to read file: ") + path);
    }

    return data;
}

static void print_shader_log(GLuint obj, bool program) {
    GLint len = 0;
    if (program) {
        glGetProgramiv(obj, GL_INFO_LOG_LENGTH, &len);
    } else {
        glGetShaderiv(obj, GL_INFO_LOG_LENGTH, &len);
    }

    if (len > 1) {
        std::string log(len, '\0');
        if (program) {
            glGetProgramInfoLog(obj, len, nullptr, log.data());
        } else {
            glGetShaderInfoLog(obj, len, nullptr, log.data());
        }
        std::cerr << log << "\n";
    }
}

static GLuint create_compute_program_from_spirv(const char* spv_path) {
    std::vector<char> spirv = read_binary_file(spv_path);

    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

    glShaderBinary(
        1,
        &shader,
        GL_SHADER_BINARY_FORMAT_SPIR_V,
        spirv.data(),
        static_cast<GLsizei>(spirv.size())
    );

    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        std::cerr << "glShaderBinary failed, GL error = 0x"
                  << std::hex << err << std::dec << "\n";
        glDeleteShader(shader);
        return 0;
    }

    glSpecializeShader(shader, "main", 0, nullptr, nullptr);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    print_shader_log(shader, false);

    if (!ok) {
        std::cerr << "SPIR-V shader specialization failed\n";
        glDeleteShader(shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, shader);
    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    print_shader_log(program, true);

    glDeleteShader(shader);

    if (!ok) {
        std::cerr << "Program link failed\n";
        glDeleteProgram(program);
        return 0;
    }

    return program;
}

static void print_double_with_bits(const char* name, double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));

    std::cout << std::setprecision(17);
    std::cout << name << " = " << value << "\n";
    std::cout << name << " bits = 0x"
              << std::hex << std::setw(16) << std::setfill('0') << bits
              << std::dec << "\n";
}

int main() {
    try {
        if (!glfwInit()) {
            throw std::runtime_error("glfwInit failed");
        }

        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        GLFWwindow* window = glfwCreateWindow(64, 64, "spirv-test", nullptr, nullptr);
        if (!window) {
            glfwTerminate();
            throw std::runtime_error("glfwCreateWindow failed");
        }

        glfwMakeContextCurrent(window);

        glewExperimental = GL_TRUE;
        GLenum glew_err = glewInit();
        glGetError(); // clear GLEW's harmless error on core profile
        if (glew_err != GLEW_OK) {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error(
                std::string("glewInit failed: ") +
                reinterpret_cast<const char*>(glewGetErrorString(glew_err))
            );
        }

        std::cout << "GL_VENDOR   = " << glGetString(GL_VENDOR) << "\n";
        std::cout << "GL_RENDERER = " << glGetString(GL_RENDERER) << "\n";
        std::cout << "GL_VERSION  = " << glGetString(GL_VERSION) << "\n\n";

        bool has_spirv = GLEW_VERSION_4_6 || glewIsSupported("GL_ARB_gl_spirv");
        if (!has_spirv) {
            glfwDestroyWindow(window);
            glfwTerminate();
            throw std::runtime_error("OpenGL SPIR-V is not supported on this driver/context");
        }

        GLuint program = create_compute_program_from_spirv("/home/spectre/Projects/test_open_3d/shader.comp.spv");
        if (!program) {
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }

        GLuint ssbo = 0;
        glGenBuffers(1, &ssbo);
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);

        TestOutputCPU init_data{};
        init_data.out_val[0] = -1.0;
        init_data.out_val[1] = -1.0;
        init_data.out_val[2] = -1.0;
        init_data.out_val[3] = -1.0;

        glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(TestOutputCPU), &init_data, GL_DYNAMIC_COPY);
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, ssbo);

        glUseProgram(program);
        glDispatchCompute(1, 1, 1);

        glMemoryBarrier(GL_ALL_BARRIER_BITS);
        glFinish();

        TestOutputCPU result{};
        glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(TestOutputCPU), &result);

        print_double_with_bits("out_val.x", result.out_val[0]);
        print_double_with_bits("out_val.y", result.out_val[1]);
        print_double_with_bits("out_val.z", result.out_val[2]);
        print_double_with_bits("out_val.w", result.out_val[3]);

        glDeleteBuffers(1, &ssbo);
        glDeleteProgram(program);
        glfwDestroyWindow(window);
        glfwTerminate();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}