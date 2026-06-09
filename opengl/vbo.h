#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <cstring>

class SSBO;
class VBO{
public:
    size_t size_bytes = 0;
    size_t capacity_bytes = 0;
    GLuint id = 0;
    GLenum usage = GL_STATIC_DRAW;

    VBO() = default;
    VBO(const void* data, size_t size_bytes, GLenum usage = GL_STATIC_DRAW);
    VBO(GLuint id, size_t size_bytes);
    VBO(const SSBO& ssbo);
    ~VBO();
    
    VBO(const VBO&) = delete;
    VBO& operator=(const VBO&) = delete;
    VBO(VBO&& o) noexcept;
    VBO& operator=(VBO&& o) noexcept;

    // void update(const void* data, size_t new_size_bytes, GLenum usage = GL_DYNAMIC_DRAW);
    void update_mapped(const void* data, size_t new_size_bytes, GLenum usage = GL_DYNAMIC_DRAW);
    void bind() const;
    static void unbind();
};
