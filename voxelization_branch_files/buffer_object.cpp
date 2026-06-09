#include "buffer_object.h"

BufferObject BufferObject::prefab_buffer;
std::mutex BufferObject::prefab_buffer_mutex;

BufferObject::BufferObject(std::size_t size_bytes, GLenum usage, const void* data) { 
    if (size_bytes == 0) {
        std::string message = "BufferObject: size_bytes must be > 0";
        std::cout << message << std::endl;
        throw std::invalid_argument(message);
    }

    glCreateBuffers(1, &id_);

    if (id_ == 0) {
        std::string message = "BufferObject: glGenBuffers failed (no GL context?)";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    usage_ = usage;
    size_bytes_ = size_bytes;

    realloc(size_bytes, usage, data);
}

BufferObject::BufferObject(std::filesystem::path path_to_BufferObject_dump_file) {
    read_binary_dump(path_to_BufferObject_dump_file);
}

BufferObject::~BufferObject() { 
    glDeleteBuffers(1, &id_); 
}

void BufferObject::move_from(BufferObject&& other) {
    id_ = other.id_;
    size_bytes_ = other.size_bytes_;
    usage_ = other.usage_;

    other.id_ = 0;
    other.size_bytes_ = 0;
}

BufferObject::BufferObject(BufferObject&& other) noexcept {
    move_from(std::move(other));
}

BufferObject& BufferObject::operator=(BufferObject&& other) noexcept {
    if (this != &other) {
        glDeleteBuffers(1, &id_);
        move_from(std::move(other));
    }

    return *this;
}

std::filesystem::path BufferObject::make_binary_dump(const std::filesystem::path& file_path) const {
    std::vector<std::byte> buffer(size_bytes_);
    read_subdata(0, size_bytes_, buffer.data());

    std::ofstream out(file_path, std::ios_base::binary);
    if (!out) {
        std::string message = "Failed to open file";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    out.write(reinterpret_cast<const char*>(&size_bytes_), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(&usage_), sizeof(GLenum));
    out.write(reinterpret_cast<const char*>(buffer.data()), size_bytes_);
    out.close();

    return file_path;
}

void BufferObject::read_binary_dump(const std::filesystem::path& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in) {
        std::string message = "Failed to open file";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }
    
    in.read(reinterpret_cast<char*>(&size_bytes_), sizeof(size_t));
    in.read(reinterpret_cast<char*>(&usage_), sizeof(GLenum));

    std::vector<std::byte> buffer(size_bytes_);
    in.read(reinterpret_cast<char*>(buffer.data()), size_bytes_);

    ensure_capacity(size_bytes_);
    update_subdata(0, size_bytes_, buffer.data());
}

void BufferObject::realloc(GLsizeiptr size_bytes, GLenum usage, const void* data) {
    if (id_ == 0u)
        glCreateBuffers(1, &id_);
    
    glNamedBufferData(id_, size_bytes, data, usage);
    usage_ = usage;
    size_bytes_ = size_bytes;
}

void BufferObject::ensure_capacity(GLsizeiptr min_size_bytes) {
    if (size_bytes_ < min_size_bytes) realloc(min_size_bytes, usage_);
}

void BufferObject::bind(GLenum target) const {
    glBindBuffer(target, id_);
}

void BufferObject::bind_base(GLenum target, GLuint index) const {
    glBindBufferBase(target, index, id_);
}

void BufferObject::bind_range(GLenum target, GLuint index, GLintptr offset, GLsizeiptr size) const {
    glBindBufferRange(target, index, id_, offset, size);
}

void BufferObject::bind_as_vbo() const { bind(GL_ARRAY_BUFFER); }
void BufferObject::bind_as_ebo() const { bind(GL_ELEMENT_ARRAY_BUFFER); }
void BufferObject::bind_as_ssbo() const { bind(GL_SHADER_STORAGE_BUFFER); }
void BufferObject::bind_as_ubo() const { bind(GL_UNIFORM_BUFFER); }

void BufferObject::bind_base_as_vbo(GLuint index) const { bind_base(GL_ARRAY_BUFFER, index); }
void BufferObject::bind_base_as_ebo(GLuint index) const { bind_base(GL_ELEMENT_ARRAY_BUFFER, index); }
void BufferObject::bind_base_as_ssbo(GLuint index) const { bind_base(GL_SHADER_STORAGE_BUFFER, index); }
void BufferObject::bind_base_as_ubo(GLuint index) const { bind_base(GL_UNIFORM_BUFFER, index); }

void BufferObject::bind_range_as_vbo(GLuint index, GLintptr offset, GLsizeiptr size) const { bind_range(GL_ARRAY_BUFFER, index, offset, size); }
void BufferObject::bind_range_as_ebo(GLuint index, GLintptr offset, GLsizeiptr size) const { bind_range(GL_ELEMENT_ARRAY_BUFFER, index, offset, size); }
void BufferObject::bind_range_as_ssbo(GLuint index, GLintptr offset, GLsizeiptr size) const { bind_range(GL_SHADER_STORAGE_BUFFER, index, offset, size); }
void BufferObject::bind_range_as_ubo(GLuint index, GLintptr offset, GLsizeiptr size) const { bind_range(GL_UNIFORM_BUFFER, index, offset, size); }

void BufferObject::update_subdata(GLintptr offset_bytes, GLsizeiptr size_bytes, const void* data) {
    if (!data || size_bytes == 0) return;
    if (offset_bytes + size_bytes > size_bytes_) {
        std::string message = "BufferObject::update_subdata: write out of buffer capacity";
        std::cout << message << std::endl;
        throw std::out_of_range(message);
    }

    glNamedBufferSubData(id_, offset_bytes, size_bytes, data);
}

void BufferObject::read_subdata(GLintptr offset_bytes, GLsizeiptr size_bytes, void* out) const {
    if (!out || size_bytes == 0) return;
    if (offset_bytes + size_bytes > size_bytes_) {
        std::string message = "BufferObject::read_subdata: out of range";
        std::cout << message << std::endl;
        throw std::out_of_range(message);
    }

    glGetNamedBufferSubData(id_, offset_bytes, size_bytes, out);
}

GLuint BufferObject::id() const { 
    return id_; 
}

std::size_t BufferObject::size_bytes() const {
    return size_bytes_;
}