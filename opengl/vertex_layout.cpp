#include "vertex_layout.h"


void VertexLayout::add(const VertexAttribute& attr) {
    attributes.push_back(attr);
}

bool VertexLayout::is_integer_type(GLenum t) {
    switch (t) {
        case GL_BYTE:
        case GL_UNSIGNED_BYTE:
        case GL_SHORT:
        case GL_UNSIGNED_SHORT:
        case GL_INT:
        case GL_UNSIGNED_INT:
            return true;
        default:
            return false;
    }
}

void VertexLayout::apply() const {
 for (const auto& a : attributes) {
        glEnableVertexAttribArray(a.index);

        if (is_integer_type(a.type)) {
            glVertexAttribIPointer(
                a.index,
                a.size,
                a.type,
                a.stride,
                reinterpret_cast<const void*>(a.offset)
            );
        } else {
            glVertexAttribPointer(
                a.index,
                a.size,
                a.type,
                a.normalized,
                a.stride,
                reinterpret_cast<const void*>(a.offset)
            );
        }

        if (a.divisor > 0) {
            glVertexAttribDivisor(a.index, a.divisor);
        }
    }
}

size_t VertexLayout::find_attribute_id_by_name(std::string attribute_name) const {
    for (size_t i = 0; i < attributes.size(); i++) {
        if (attribute_name == attributes[i].name)
            return i;
    }
    return -1;
}