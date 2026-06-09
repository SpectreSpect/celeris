#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <string>
#include <chrono>
#include <type_traits>
#include <initializer_list>
#include <vector>
#include <cstddef>   // std::byte
#include <cstring>   // std::memcpy

struct VertexAttribute {
    std::string name;
    GLuint index;
    GLint size;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    GLsizei stride;
    size_t offset;
    GLuint divisor = 0;   // for instancing
    std::vector<std::byte> default_value;

    inline VertexAttribute(std::string name,
                    GLuint index,
                    GLint size,
                    GLenum type = GL_FLOAT,
                    GLboolean normalized = GL_FALSE,
                    GLsizei stride = 0,
                    size_t offset = 0,
                    GLuint divisor = 0,
                    std::vector<std::byte> default_value = {})
        : name(std::move(name)),
        index(index),
        size(size),
        type(type),
        normalized(normalized),
        stride(stride),
        offset(offset),
        divisor(divisor),
        default_value(std::move(default_value))
    {}

    template<class T>
    inline VertexAttribute& set_default(std::initializer_list<T> vals) {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        default_value.resize(vals.size() * sizeof(T));
        std::memcpy(default_value.data(), vals.begin(), vals.size() * sizeof(T));
        return *this;
    }
};