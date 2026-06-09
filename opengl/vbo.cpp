#include "vbo.h"
#include "ssbo.h"


VBO::VBO(const void* data, size_t size_bytes, GLenum usage) {
    if (size_bytes == 0) {
        throw std::invalid_argument("VBO: size == 0");
    }
    glGenBuffers(1, &id);
    if (id == 0)
        throw std::runtime_error("VBO: glGenBuffers failed (no GL context?)");
    this->bind();
    glBufferData(GL_ARRAY_BUFFER, size_bytes, data, usage);
    this->unbind();

    this->size_bytes = size_bytes;
    this->capacity_bytes = size_bytes;
}

VBO::VBO(GLuint id, size_t size_bytes) {
    if (size_bytes == 0)
        throw std::invalid_argument("VBO: size == 0");

    if (id == 0)
        throw std::runtime_error("VBO: glGenBuffers failed (no GL context?)");

    this->id = id;
    this->size_bytes = size_bytes;
    this->capacity_bytes = size_bytes;
}

VBO::VBO(const SSBO& ssbo) {
    this->id = ssbo.id_;
    this->size_bytes = ssbo.size_bytes();
    this->capacity_bytes = ssbo.capacity_bytes();
    // this->capacity_bytes = ssbo.size_bytes();
    this->usage = ssbo.usage();
}

VBO::~VBO() {
    if (id != 0)
        glDeleteBuffers(1, &id);
}

VBO::VBO(VBO&& o) noexcept {
    size_bytes = o.size_bytes;
    capacity_bytes = o.capacity_bytes;
    id = o.id;

    o.size_bytes = 0;
    o.capacity_bytes = 0;
    o.id = 0;
}

VBO& VBO::operator=(VBO&& o) noexcept {
    if (this != &o) {
        if (id != 0)
            glDeleteBuffers(1, &id);
        
        size_bytes = o.size_bytes;
        capacity_bytes = o.capacity_bytes;
        id = o.id;

        o.size_bytes = 0;
        o.capacity_bytes = 0;
        o.id = 0;
    }
    
    return *this;
}

void VBO::update_mapped(const void* data, size_t new_size_bytes, GLenum usage) {
    this->usage = usage;
    if (!data || new_size_bytes == 0)
        return;

    this->bind();

    if (new_size_bytes > capacity_bytes) {
        size_t new_capacity = std::max(new_size_bytes, capacity_bytes ? capacity_bytes * 2 : new_size_bytes);
        glBufferData(GL_ARRAY_BUFFER, new_capacity, nullptr, usage); // orphan
        capacity_bytes = new_capacity;
    }

    void* ptr = glMapBufferRange(
        GL_ARRAY_BUFFER,
        0,
        new_size_bytes,
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT | GL_MAP_UNSYNCHRONIZED_BIT
    );

    if (ptr) {
        memcpy(ptr, data, new_size_bytes);
        glUnmapBuffer(GL_ARRAY_BUFFER);   
    } else {
        glBufferSubData(GL_ARRAY_BUFFER, 0, new_size_bytes, data);
    }

    this->size_bytes = new_size_bytes;
    this->unbind();
}

void VBO::bind() const {
    glBindBuffer(GL_ARRAY_BUFFER, id);
}

void VBO::unbind() {
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}