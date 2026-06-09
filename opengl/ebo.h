#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <cstring>

class SSBO;
class EBO{
public:
    GLuint id = 0;
    size_t size_bytes = 0;
    size_t num_indices = 0;
    size_t capacity_bytes = 0;
    GLenum usage = GL_STATIC_DRAW;

    EBO() = default;
    EBO(const void* indices, size_t size_bytes);
    EBO(GLuint id, size_t size_bytes);
    EBO(const SSBO& ssbo);
    ~EBO();

    EBO(const EBO&) = delete;
    EBO& operator=(const EBO&) = delete;
    EBO(EBO&& o) noexcept;
    EBO& operator=(EBO&& o) noexcept;

    void update_mapped(const void* data, size_t new_size_bytes, GLenum usage);
    void bind() const;
    static void unbind();
};