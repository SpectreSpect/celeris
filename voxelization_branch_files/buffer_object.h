#pragma once
#include <GL/glew.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <iostream>
#include <thread>
#include <mutex>

#include "shader_manager.h"
#include "config.h"

class BufferObject {
public:
    static BufferObject prefab_buffer;
    static std::mutex prefab_buffer_mutex;

    BufferObject() noexcept = default;
    BufferObject(std::size_t size_bytes, GLenum usage, const void* data = nullptr);
    BufferObject(std::filesystem::path path_to_BufferObject_dump_file);
    ~BufferObject();

    BufferObject(const BufferObject&) = delete;
    BufferObject& operator=(const BufferObject&) = delete;

    void move_from(BufferObject&& other);
    BufferObject(BufferObject&& other) noexcept;
    BufferObject& operator=(BufferObject&& other) noexcept;

    template<class T>
    static inline BufferObject from_fill(
        size_t offset_bytes,
        size_t size_bytes, 
        GLenum usage, 
        T fill_value, 
        ComputeProgram& clear_buffer_prog, 
        BufferObject& prefab_buffer = BufferObject::prefab_buffer,
        uint32_t invocation_stride = 4u) 
    {
        if (size_bytes == sizeof(T)) {
            // Короткий путь
            return BufferObject(size_bytes, usage, &fill_value);
        }

        BufferObject BufferObject(size_bytes, usage);
        BufferObject.update_subdata_fill(offset_bytes, fill_value, size_bytes, clear_buffer_prog, prefab_buffer, invocation_stride);

        return BufferObject;
    }

    template<class T>
    static inline BufferObject from_fill(
        size_t offset_bytes,
        size_t size_bytes, 
        GLenum usage, 
        T fill_value, 
        ShaderManager& shader_manager, 
        BufferObject& prefab_buffer = BufferObject::prefab_buffer,
        uint32_t invocation_stride = 4u)
    {
        return BufferObject::from_fill(offset_bytes, size_bytes, usage, fill_value, shader_manager.clear_buffer_prog, prefab_buffer, invocation_stride);
    }

    template<class T>
    static inline BufferObject from_fill(
        size_t size_bytes, 
        GLenum usage, 
        T fill_value, 
        ShaderManager& shader_manager, 
        BufferObject& prefab_buffer = BufferObject::prefab_buffer,
        uint32_t invocation_stride = 4u)
    {
        return BufferObject::from_fill(0, size_bytes, usage, fill_value, shader_manager.clear_buffer_prog, prefab_buffer, invocation_stride);
    }

    template<class T>
    static inline BufferObject from_fill(
        size_t size_bytes, 
        GLenum usage, 
        T fill_value, 
        ComputeProgram& clear_buffer_prog, 
        BufferObject& prefab_buffer = BufferObject::prefab_buffer,
        uint32_t invocation_stride = 4u)
    {
        return BufferObject::from_fill(0, size_bytes, usage, fill_value, clear_buffer_prog, prefab_buffer, invocation_stride);
    }

    std::filesystem::path make_binary_dump(const std::filesystem::path& file_path) const;
    void read_binary_dump(const std::filesystem::path& file_path);

    void realloc(GLsizeiptr size_bytes, GLenum usage, const void* data = nullptr);
    void ensure_capacity(GLsizeiptr min_size_bytes);
    void bind(GLenum target) const;
    void bind_base(GLenum target, GLuint index) const;
    void bind_range(GLenum target, GLuint index, GLintptr offset, GLsizeiptr size) const;

    void bind_as_vbo() const;
    void bind_as_ebo() const;
    void bind_as_ssbo() const;
    void bind_as_ubo() const;

    void bind_base_as_vbo(GLuint index) const;
    void bind_base_as_ebo(GLuint index) const;
    void bind_base_as_ssbo(GLuint index) const;
    void bind_base_as_ubo(GLuint index) const;

    void bind_range_as_vbo(GLuint index, GLintptr offset, GLsizeiptr size) const;
    void bind_range_as_ebo(GLuint index, GLintptr offset, GLsizeiptr size) const;
    void bind_range_as_ssbo(GLuint index, GLintptr offset, GLsizeiptr size) const;
    void bind_range_as_ubo(GLuint index, GLintptr offset, GLsizeiptr size) const;
    
    void update_subdata(GLintptr offset_bytes, GLsizeiptr size_bytes, const void* data);

    template<class T>
    void update_subdata_fill(
        uint32_t offset_bytes, 
        T fill_value, 
        uint32_t size_bytes, 
        ComputeProgram& clear_buffer_prog, 
        BufferObject& prefab_buffer = BufferObject::prefab_buffer,
        uint32_t invocation_stride = 4u) 
    {
        if (size_bytes == 0u) return;

        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

        uint64_t clearable_end = uint64_t(offset_bytes) + uint64_t(size_bytes);
        if (clearable_end > size_bytes_) {
            std::string clearable_buffer_size_str = std::to_string(clearable_end);
            std::string buffer_size_str = std::to_string(size_bytes_);
            std::string message = "The clearable area exceeded the buffer size (" + clearable_buffer_size_str + " > " + buffer_size_str + ")";
            std::cout << message << std::endl;
            throw std::runtime_error(message);
        }

        if (invocation_stride == 0u) {
            std::string message = "invocation_stride must not be equal to 0";
            std::cout << message << std::endl;
            throw std::runtime_error(message);
        }

        // Короткий путь
        if (size_bytes == sizeof(T)) {
            update_subdata(offset_bytes, size_bytes, &fill_value);
            return;
        }

        // Расчёт параметров
        uint32_t total_count_invocations = math_utils::div_up_u32(size_bytes, invocation_stride);
        uint32_t count_invocations_x = 0;
        uint32_t count_invocations_y = 0;
        uint32_t count_invocations_z = 0;
        if (total_count_invocations > 0) {
            count_invocations_x = std::ceil(std::cbrt(total_count_invocations));

            uint32_t count_invocations_yz = math_utils::div_up_u32(total_count_invocations, count_invocations_x);
            count_invocations_y = std::ceil(std::sqrt(count_invocations_yz));
            count_invocations_z = math_utils::div_up_u32(count_invocations_yz, count_invocations_y);
        }

        uint32_t groups_x = math_utils::div_up_u32(count_invocations_x, 8u);
        uint32_t groups_y = math_utils::div_up_u32(count_invocations_y, 8u);
        uint32_t groups_z = math_utils::div_up_u32(count_invocations_z, 8u);

        GLint max_groups_x = 0;
        GLint max_groups_y = 0;
        GLint max_groups_z = 0;

        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &max_groups_x);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &max_groups_y);
        glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &max_groups_z);

        if (groups_x > max_groups_x || 
            groups_y > max_groups_y || 
            groups_z > max_groups_z) {
            std::string x  = std::to_string(groups_x);
            std::string y  = std::to_string(groups_y);
            std::string z  = std::to_string(groups_z);

            std::string xm = std::to_string(max_groups_x);
            std::string ym = std::to_string(max_groups_y);
            std::string zm = std::to_string(max_groups_z);
            std::string message = "The value of dispatch group count (" + x + ", " + y + ", " + z + ") was out of bounds (" + xm + ", " + ym + ", " + zm + ")";
            std::cout << message << std::endl;
            throw std::runtime_error(message);
        }

        std::unique_lock<std::mutex> lock(prefab_buffer_mutex, std::defer_lock);
        if (&prefab_buffer == &this->prefab_buffer) {
            lock.lock(); // Чтобы не возникало проблем в случае многопоточности

            if constexpr (!config::allow_lock_BufferObject) {
                std::cout << "BufferObject is blocking threads! Pass your BufferObject prefab_buffer to "
                            "BufferObject::update_subdata_fill() or BufferObject::from_fill() to avoid blocking. "
                            "If you want to suppress this message, set config::allow_lock_BufferObject = true."
                        << std::endl;
            }
        }

        // Инициализация буффера префаба
        uint32_t min_prefab_capacity_bytes = (sizeof(T) + 3u) & ~3u;
        if (prefab_buffer.size_bytes() < min_prefab_capacity_bytes) {
            std::byte* initial_data = new std::byte[min_prefab_capacity_bytes]{};
            std::memcpy(initial_data, &fill_value, sizeof(T));

            prefab_buffer = BufferObject(min_prefab_capacity_bytes, GL_DYNAMIC_DRAW, initial_data);
            delete[] initial_data;
        } else {
            prefab_buffer.update_subdata(0, sizeof(T), &fill_value);
        }

        // Вызов шейдера
        prefab_buffer.bind_base_as_ssbo(0);
        this->bind_base_as_ssbo(1); // Прикрепляем себя как очищаемый буффер

        clear_buffer_prog.use();

        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "prefab_data_bytes"), (uint32_t)(sizeof(T)));
        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "clearable_data_bytes"), size_bytes);

        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "clearable_data_offset_bytes"), offset_bytes);

        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "count_invocations_x"), count_invocations_x);
        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "count_invocations_y"), count_invocations_y);
        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "count_invocations_z"), count_invocations_z);

        glUniform1ui(glGetUniformLocation(clear_buffer_prog.id, "invocation_stride_bytes"), invocation_stride);

        clear_buffer_prog.dispatch_compute(groups_x, groups_y, groups_z);
        glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
    }

    template<class T>
    void update_subdata_fill(
        uint32_t offset_bytes, 
        T fill_value, 
        uint32_t size_bytes, 
        ShaderManager& shader_manager, 
        BufferObject& prefab_buffer = BufferObject::prefab_buffer,
        uint32_t invocation_stride = 4u)  
    {
        update_subdata_fill(offset_bytes, fill_value, size_bytes, shader_manager.clear_buffer_prog, prefab_buffer, invocation_stride);
    }

    void read_subdata(GLintptr offset_bytes, GLsizeiptr size_bytes, void* out) const;

    template<class T>
    T read_scalar(std::size_t offset_bytes = 0) const {
        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
        T v{};
        read_subdata(offset_bytes, sizeof(T), &v);
        return v;
    }

    GLuint id() const;
    std::size_t size_bytes() const;

private:
    GLuint id_ = 0;
    std::size_t size_bytes_ = 0;
    GLenum usage_ = GL_DYNAMIC_DRAW;
};