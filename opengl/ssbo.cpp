// BufferObject.cpp
#include "BufferObject.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

BufferObject BufferObject::prefab_buffer;
std::mutex BufferObject::prefab_buffer_mutex;

static void require_gl_buffer_storage() {
    // Для persistent нужен OpenGL 4.4 или расширение ARB_buffer_storage
    if (!(GLEW_VERSION_4_4 || GLEW_ARB_buffer_storage)) {
        throw std::runtime_error("BufferObject persistent mapping requires OpenGL 4.4 or GL_ARB_buffer_storage");
    }
}

BufferObject::BufferObject(std::size_t size_bytes, GLenum usage, const void* initial_data) {
    init_with_initial_data(size_bytes, usage, initial_data);
}

BufferObject::BufferObject(std::filesystem::path path_to_BufferObject_dump_file) {
    read_binary_dump(path_to_BufferObject_dump_file);
}

void BufferObject::init_with_initial_data(std::size_t size_bytes, GLenum usage, const void* initial_data) {
    if (size_bytes == 0) {
        throw std::invalid_argument("BufferObject: size_bytes must be > 0");
    }

    glGenBuffers(1, &id_);
    if (id_ == 0) {
        throw std::runtime_error("BufferObject: glGenBuffers failed (no GL context?)");
    }

    usage_ = usage;
    size_bytes_ = size_bytes;
    capacity_bytes_ = size_bytes;

    bind();
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity_bytes_),
                 initial_data,
                 usage_);
    unbind();
}

<<<<<<<< HEAD:voxel_engine/buffer_objects/ssbo.cpp
BufferObject BufferObject::create_persistent(std::size_t size_bytes,
========
SSBO::SSBO(const VBO& vbo) {
    set_id(vbo.id);


    size_bytes_ = vbo.size_bytes;      // "логический" размер данных
    capacity_bytes_ = vbo.capacity_bytes;  // реально выделенный размер
    usage_ = GL_DYNAMIC_DRAW;
}

SSBO SSBO::create_persistent(std::size_t size_bytes,
>>>>>>>> origin/cloud-to-mesh:opengl/ssbo.cpp
                            GLbitfield storage_flags,
                            GLbitfield map_flags,
                            const void* initial_data)
{
    require_gl_buffer_storage();

    if (size_bytes == 0) {
        throw std::invalid_argument("BufferObject: size_bytes must be > 0");
    }
    if ((storage_flags & GL_MAP_PERSISTENT_BIT) == 0 || (map_flags & GL_MAP_PERSISTENT_BIT) == 0) {
        throw std::invalid_argument("BufferObject persistent: GL_MAP_PERSISTENT_BIT must be set in both storage_flags and map_flags");
    }
    if ((map_flags & GL_MAP_WRITE_BIT) == 0) {
        throw std::invalid_argument("BufferObject persistent: map_flags must include GL_MAP_WRITE_BIT (for uploads)");
    }

    BufferObject b;
    glGenBuffers(1, &b.id_);
    if (b.id_ == 0) {
        throw std::runtime_error("BufferObject: glGenBuffers failed (no GL context?)");
    }

    b.usage_ = GL_DYNAMIC_DRAW; // usage тут не используется (bufferStorage), но оставим “разумное”
    b.size_bytes_ = size_bytes;
    b.capacity_bytes_ = size_bytes;
    b.storage_flags_ = storage_flags;
    b.map_flags_ = map_flags;

    b.bind();
    glBufferStorage(GL_SHADER_STORAGE_BUFFER,
                    static_cast<GLsizeiptr>(b.capacity_bytes_),
                    initial_data,
                    storage_flags);

    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                0,
                                static_cast<GLsizeiptr>(b.capacity_bytes_),
                                map_flags);
    if (!ptr) {
        b.unbind();
        b.destroy();
        throw std::runtime_error("BufferObject persistent: glMapBufferRange failed");
    }

    b.mapped_ptr_ = ptr;
    b.unbind();

    return b;
}

BufferObject::~BufferObject() {
    destroy();
}

void BufferObject::destroy() noexcept {
    if (id_ != 0) {
        // Если буфер маппился — аккуратно размэпим
        // (в persistent режиме это допустимо сделать перед удалением)
        if (mapped_ptr_) {
            bind();
            glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
            unbind();
            mapped_ptr_ = nullptr;
        }

        glDeleteBuffers(1, &id_);
        id_ = 0;
    }
    size_bytes_ = 0;
    capacity_bytes_ = 0;
    usage_ = GL_DYNAMIC_DRAW;
    storage_flags_ = 0;
    map_flags_ = 0;
}

BufferObject::BufferObject(BufferObject&& other) noexcept {
    move_from(std::move(other));
}

BufferObject& BufferObject::operator=(BufferObject&& other) noexcept {
    if (this != &other) {
        destroy();
        move_from(std::move(other));
    }
    return *this;
}

void BufferObject::move_from(BufferObject&& other) noexcept {
    id_ = other.id_;
    size_bytes_ = other.size_bytes_;
    capacity_bytes_ = other.capacity_bytes_;
    usage_ = other.usage_;
    mapped_ptr_ = other.mapped_ptr_;
    storage_flags_ = other.storage_flags_;
    map_flags_ = other.map_flags_;

    other.id_ = 0;
    other.size_bytes_ = 0;
    other.capacity_bytes_ = 0;
    other.mapped_ptr_ = nullptr;
    other.storage_flags_ = 0;
    other.map_flags_ = 0;
}

void BufferObject::bind() const noexcept {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id_);
}

std::filesystem::path BufferObject::make_csv_dump(
    const std::filesystem::path& path,
    const std::map<std::string, DumpType>& structure_field_sizes_bytes,
    size_t offset_bytes,
    size_t count_structures) const
{
    // ------ Configuration / предположения ------
    // Размеры типов (в байтах). При необходимости поменяйте BOOL_SIZE.
    constexpr size_t UINT_SIZE  = 4;
    constexpr size_t INT_SIZE   = 4;
    constexpr size_t FLOAT_SIZE = 4;
    constexpr size_t BOOL_SIZE  = 4; // часто в BufferObject булевы поля выравниваются на 4 байта

    // ----- Проверки -----
    if (count_structures == 0) {
        throw std::invalid_argument("count_structures must be > 0");
    }
    if (structure_field_sizes_bytes.empty()) {
        throw std::invalid_argument("structure_field_sizes_bytes must not be empty");
    }

    // ----- Рассчитать размер одной структуры (в байтах) -----
    size_t struct_size = 0;
    for (const auto& kv : structure_field_sizes_bytes) {
        switch (kv.second) {
            case UINT:  struct_size += UINT_SIZE;  break;
            case INT:   struct_size += INT_SIZE;   break;
            case FLOAT: struct_size += FLOAT_SIZE; break;
            case BOOL:  struct_size += BOOL_SIZE;  break;
            default:
                throw std::invalid_argument("unknown DumpType value");
        }
    }

    if (struct_size == 0) {
        throw std::runtime_error("computed structure size is zero");
    }

    // ----- Подготовка пути к файлу -----
    std::filesystem::path out_file = path;
    try {
        if (std::filesystem::exists(path) && std::filesystem::is_directory(path)) {
            // сформируем имя файла с временной меткой
            auto now = std::chrono::system_clock::now();
            std::time_t tnow = std::chrono::system_clock::to_time_t(now);
            std::tm tm;
#if defined(_WIN32)
            localtime_s(&tm, &tnow);
#else
            localtime_r(&tnow, &tm);
#endif
            std::ostringstream name;
            name << "BufferObject_dump_"
                 << std::put_time(&tm, "%Y%m%d_%H%M%S")
                 << ".csv";
            out_file /= name.str();
        } else {
            // если путь не существует — попробуем создать родительские директории
            if (out_file.has_parent_path()) {
                std::filesystem::create_directories(out_file.parent_path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        throw std::runtime_error(std::string("filesystem error: ") + e.what());
    }

    // ----- Открыть файл для записи -----
    std::ofstream ofs(out_file, std::ios::binary);
    if (!ofs.is_open()) {
        throw std::runtime_error("failed to open output file for writing: " + out_file.string());
    }

    // ----- Записать заголовок CSV (имена полей в порядке map) -----
    bool first = true;
    for (const auto& kv : structure_field_sizes_bytes) {
        if (!first) ofs << ',';
        first = false;
        // оборачиваем имя в кавычки на случай запятых
        ofs << '"' << kv.first << '"';
    }
    ofs << '\n';

    // ----- Буфер для одной структуры -----
    std::vector<uint8_t> struct_buf(struct_size);

    // ----- Для каждой структуры: читать и распарсить -----
    for (size_t i = 0; i < count_structures; ++i) {
        size_t read_offset = offset_bytes + i * struct_size;

        // читаем одну структуру
        read_subdata(read_offset, struct_buf.data(), struct_size);

        // парсим поля по очереди
        size_t local_off = 0;
        bool first_value = true;
        for (const auto& kv : structure_field_sizes_bytes) {
            if (!first_value) ofs << ',';
            first_value = false;

            DumpType dt = kv.second;
            switch (dt) {
                case UINT: {
                    uint32_t v = 0;
                    // безопасно копируем независимо от выравнивания
                    std::memcpy(&v, struct_buf.data() + local_off, UINT_SIZE);
                    ofs << v;
                    local_off += UINT_SIZE;
                    break;
                }
                case INT: {
                    int32_t v = 0;
                    std::memcpy(&v, struct_buf.data() + local_off, INT_SIZE);
                    ofs << v;
                    local_off += INT_SIZE;
                    break;
                }
                case FLOAT: {
                    float f = 0.0f;
                    std::memcpy(&f, struct_buf.data() + local_off, FLOAT_SIZE);
                    // форматирование float: достаточно 6-9 значащих цифр
                    ofs << std::setprecision(9) << f;
                    // вернуть формат по умолчанию (не обязательно, но аккуратно)
                    ofs << std::setprecision(6);
                    local_off += FLOAT_SIZE;
                    break;
                }
                case BOOL: {
                    // по умолчанию читаем как 32-битное целое (см. BOOL_SIZE)
                    if (BOOL_SIZE == 1) {
                        uint8_t b = 0;
                        std::memcpy(&b, struct_buf.data() + local_off, 1);
                        ofs << (b ? "true" : "false");
                        local_off += 1;
                    } else if (BOOL_SIZE == 4) {
                        uint32_t b = 0;
                        std::memcpy(&b, struct_buf.data() + local_off, 4);
                        ofs << (b ? "true" : "false");
                        local_off += 4;
                    } else {
                        // на всякий случай поддерживаем произвольный размер булева поля
                        uint32_t b = 0;
                        std::memcpy(&b, struct_buf.data() + local_off, std::min<size_t>(4, BOOL_SIZE));
                        ofs << (b ? "true" : "false");
                        local_off += BOOL_SIZE;
                    }
                    break;
                }
                default:
                    throw std::runtime_error("unhandled DumpType while writing CSV");
            }
        } // fields

        ofs << '\n';
    } // structures

    ofs.close();
    if (!ofs) {
        throw std::runtime_error("error occurred while writing CSV file: " + out_file.string());
    }

    return out_file;
}

std::filesystem::path BufferObject::make_binary_dump(const std::filesystem::path& file_path) const {
    std::vector<std::byte> buffer(size_bytes_);
    read_subdata(0, buffer.data(), size_bytes_);

    std::ofstream out(file_path, std::ios_base::binary);
    if (!out)
        throw std::runtime_error("Failed to open file");

    out.write(reinterpret_cast<const char*>(&size_bytes_), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(&capacity_bytes_), sizeof(size_t));
    out.write(reinterpret_cast<const char*>(&usage_), sizeof(GLenum));
    out.write(reinterpret_cast<const char*>(buffer.data()), size_bytes_);
    out.close();

    return file_path;
}

void BufferObject::read_binary_dump(const std::filesystem::path& file_path) {
    std::ifstream in(file_path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Failed to open file");
    
    in.read(reinterpret_cast<char*>(&size_bytes_), sizeof(size_t));
    in.read(reinterpret_cast<char*>(&capacity_bytes_), sizeof(size_t));
    in.read(reinterpret_cast<char*>(&usage_), sizeof(GLenum));

    // std::cout << "size_bytes: " << size_bytes_ << std::endl;
    // std::cout << "capacity_bytes: " << capacity_bytes_ << std::endl;
    // std::cout << "usage: " << usage_ << std::endl;
    // std::cout << std::endl;

    std::vector<std::byte> buffer(size_bytes_);
    in.read(reinterpret_cast<char*>(buffer.data()), size_bytes_);

    // size_t count_print = std::min(10ull, size_bytes_ / sizeof(uint32_t));
    // for (size_t i = 0; i < count_print; i++) {
    //     std::cout << *(uint32_t*)(buffer.data() + i) << " ";
    // }
    // std::cout << std::endl;

    update_subdata(0, buffer.data(), size_bytes_);
}

void BufferObject::unbind() noexcept {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
}

void BufferObject::bind_base(GLuint binding) const noexcept {
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, binding, id_);
}

void BufferObject::bind_range(GLuint binding, std::size_t offset_bytes, std::size_t size_bytes) const noexcept {
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER,
                      binding,
                      id_,
                      static_cast<GLintptr>(offset_bytes),
                      static_cast<GLsizeiptr>(size_bytes));
}

<<<<<<<< HEAD:voxel_engine/buffer_objects/ssbo.cpp
void BufferObject::realloc(std::size_t new_capacity_bytes, GLenum usage, const void* data) {
========
void SSBO::clear() {
    GLuint zero = 0;
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, id_);
    glClearBufferData(GL_SHADER_STORAGE_BUFFER,
                    GL_R32UI,
                    GL_RED_INTEGER,
                    GL_UNSIGNED_INT,
                    &zero);
}

void SSBO::realloc(std::size_t new_capacity_bytes, GLenum usage, const void* data) {
>>>>>>>> origin/cloud-to-mesh:opengl/ssbo.cpp
    if (new_capacity_bytes == 0) {
        throw std::invalid_argument("BufferObject::realloc: new_capacity_bytes must be > 0");
    }
    if (is_persistent()) {
        // Persistent буфер нельзя просто так realloc-нуть через glBufferData.
        // (Можно создать новый BufferObject и move-assign.)
        throw std::runtime_error("BufferObject::realloc: not supported for persistent-mapped buffers (create a new buffer and move-assign)");
    }

    usage_ = usage;
    capacity_bytes_ = new_capacity_bytes;
    size_bytes_ = std::min(size_bytes_, capacity_bytes_);

    bind();
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity_bytes_),
                 data,
                 usage_);
    unbind();
}

void BufferObject::ensure_capacity(std::size_t min_capacity_bytes) {
    if (min_capacity_bytes <= capacity_bytes_) return;
    if (is_persistent()) {
        throw std::runtime_error("BufferObject::ensure_capacity: not supported for persistent-mapped buffers (create a new larger buffer)");
    }

    std::size_t new_cap = std::max(min_capacity_bytes, capacity_bytes_ ? capacity_bytes_ * 2 : min_capacity_bytes);
    realloc(new_cap, usage_, nullptr); // orphan
}

void BufferObject::update_subdata(std::size_t offset_bytes, const void* data, std::size_t data_bytes) {
    if (!data || data_bytes == 0) return;
    if (offset_bytes + data_bytes > capacity_bytes_) {
        throw std::out_of_range("BufferObject::update_subdata: write out of buffer capacity");
    }

    bind();
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    static_cast<GLintptr>(offset_bytes),
                    static_cast<GLsizeiptr>(data_bytes),
                    data);
    unbind();

    size_bytes_ = std::max(size_bytes_, offset_bytes + data_bytes);
}

void BufferObject::update_discard(const void* data, std::size_t data_bytes) {
    if (!data || data_bytes == 0) return;

    if (is_persistent()) {
        if (data_bytes > capacity_bytes_) {
            throw std::runtime_error("BufferObject::update_discard: persistent buffer too small (create a new larger buffer and move-assign)");
        }
        std::memcpy(mapped_ptr_, data, data_bytes);
        size_bytes_ = data_bytes;

        if ((storage_flags_ & GL_MAP_COHERENT_BIT) == 0) {
            flush_range(0, data_bytes);
        }
        return;
    }

    if (data_bytes > capacity_bytes_) {
        ensure_capacity(data_bytes);
    }

    // orphan + map + memcpy (без UNSYNCHRONIZED)
    bind();

    // Если хочешь максимально избегать stall даже при одинаковом capacity:
    // orphan можно делать каждый раз при полном переписывании:
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 static_cast<GLsizeiptr>(capacity_bytes_),
                 nullptr,
                 usage_);

    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                0,
                                static_cast<GLsizeiptr>(data_bytes),
                                GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr) {
        std::memcpy(ptr, data, data_bytes);
        GLboolean ok = glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
        unbind();
        if (ok == GL_FALSE) {
            throw std::runtime_error("BufferObject::update_discard: glUnmapBuffer reported GL_FALSE");
        }
    } else {
        // fallback
        glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, static_cast<GLsizeiptr>(data_bytes), data);
        unbind();
    }

    size_bytes_ = data_bytes;
}

void* BufferObject::map_range(std::size_t offset_bytes, std::size_t length_bytes, GLbitfield access_flags) {
    if (is_persistent()) {
        // В persistent режиме смысла мапить заново нет — уже есть mapped_ptr_
        // (если всё-таки нужно — лучше сделать отдельный метод, но обычно не надо)
        return nullptr;
    }
    if (offset_bytes + length_bytes > capacity_bytes_) return nullptr;

    bind();
    void* ptr = glMapBufferRange(GL_SHADER_STORAGE_BUFFER,
                                static_cast<GLintptr>(offset_bytes),
                                static_cast<GLsizeiptr>(length_bytes),
                                access_flags);
    // NOTE: unbind делать нельзя, пока ptr используется (по-хорошему можно, но лучше не усложнять)
    return ptr;
}

bool BufferObject::unmap() {
    if (is_persistent()) return true; // persistent не размапливаем “по ходу”
    GLboolean ok = glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    unbind();
    return ok == GL_TRUE;
}

void BufferObject::flush_range(std::size_t offset_bytes, std::size_t length_bytes) {
    if (!is_persistent()) return;
    if ((storage_flags_ & GL_MAP_COHERENT_BIT) != 0) return; // coherent -> flush не нужен

    bind();
    glFlushMappedBufferRange(GL_SHADER_STORAGE_BUFFER,
                             static_cast<GLintptr>(offset_bytes),
                             static_cast<GLsizeiptr>(length_bytes));
    unbind();
}

void BufferObject::read_subdata(std::size_t offset_bytes, void* out, std::size_t out_bytes) const {
    if (!out || out_bytes == 0) return;
    if (offset_bytes + out_bytes > capacity_bytes_) {
        throw std::out_of_range("BufferObject::read_subdata: out of range");
    }
    
    bind();
    glGetBufferSubData(GL_SHADER_STORAGE_BUFFER,
                       (GLintptr)offset_bytes,
                       (GLsizeiptr)out_bytes,
                       out);
    unbind();
}

uint32_t BufferObject::read_u32(std::size_t offset_bytes) const {
    uint32_t v = 0;
    read_subdata(offset_bytes, &v, sizeof(uint32_t));
    return v;
}

GLuint BufferObject::id() const {
    return id_;
}

void SSBO::set_id(GLuint id) {
    id_ = id;
}