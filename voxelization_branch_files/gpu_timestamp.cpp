#include "gpu_timestamp.h"

GPUTimestamp::GPUTimestamp() {
    glGenQueries(1, &id);
    glQueryCounter(id, GL_TIMESTAMP);
}

GPUTimestamp::~GPUTimestamp() {
    if (id) glDeleteQueries(1, &id);
}

GPUTimestamp::GPUTimestamp(GPUTimestamp&& other) noexcept : id(other.id) {
    other.id = 0;
}

GPUTimestamp& GPUTimestamp::operator=(GPUTimestamp&& other) noexcept {
    if (this != &other) {
        if (id) glDeleteQueries(1, &id);
        id = other.id;
        other.id = 0;
    }
    return *this;
}

uint64_t GPUTimestamp::ns() const {
    GLuint64 t = 0;
    glGetQueryObjectui64v(id, GL_QUERY_RESULT, &t);
    return static_cast<std::uint64_t>(t);
}

double GPUTimestamp::operator-(const GPUTimestamp& other) const {
    return (ns() - other.ns()) / 1'000'000.0;
}