#include "vao.h"

VAO::VAO() {
    glGenVertexArrays(1, &id);
}

VAO::~VAO() {
    if (id != 0)
        glDeleteVertexArrays(1, &id);
}

VAO::VAO(VAO&& o) noexcept {
    id = o.id;
    o.id = 0;
}

VAO& VAO::operator=(VAO&& o) noexcept {
    if (this != &o) {
        if (id != 0)
            glDeleteVertexArrays(1, &id);

        id = o.id;
        o.id = 0;
    }
    
    return *this;
}

// VAO& VAO::init_vao() & {
//     glGenVertexArrays(1, &this->id);
//     if (this->id == 0) {
//         std::string message = "Failed to create VAO";
//         std::cout << message << std::endl;
//         throw std::runtime_error(message);
//     }
    
//     return *this;
// }

// VAO&& VAO::init_vao() && {
//     static_cast<VAO&>(*this).init_vao();
//     return std::move(*this);
// }


void VAO::setup(const BufferObject& vbo, const BufferObject& ebo, const VertexLayout& vertex_layout) {
    this->bind();

    vbo.bind_as_vbo();
    ebo.bind_as_ebo();
    vertex_layout.apply();

    this->unbind();
}

void VAO::bind() const {
    glBindVertexArray(id);
}

void VAO::unbind() {
    glBindVertexArray(0);
}