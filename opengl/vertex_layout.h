#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vertex_attribute.h"
#include "vector"


class VertexLayout {
public:
    std::vector<VertexAttribute> attributes;

    void add(const VertexAttribute& attr);
    
    template<class T>
    inline void add(
        std::string name,
        GLuint index,
        GLint size,
        GLenum type = GL_FLOAT,
        GLboolean normalized = GL_FALSE,
        GLsizei stride = 0,
        size_t offset = 0,
        GLuint divisor = 0,
        std::initializer_list<T> default_value = {}
    ) {
        const VertexAttribute attr = VertexAttribute(
            name, index, size, type, normalized, 
            stride, offset, divisor).set_default(default_value);
        add(attr);
    }

    static bool is_integer_type(GLenum t);
    void apply() const;
    size_t find_attribute_id_by_name(std::string attribute_name) const;
};
