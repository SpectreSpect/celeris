#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <optional>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_memory.h"
#include "vulkan_physical_device.h"
#include "vulkan_device.h"

class VulkanCommandBuffer;
class VulkanEngine;
class PassWriter;
class VulkanBufferView;

constexpr uint32_t FILL_LOCAL_SIZE_X = 8u;
constexpr uint32_t FILL_LOCAL_SIZE_Y = 8u;
constexpr uint32_t FILL_LOCAL_SIZE_Z = 8u;

class VulkanBuffer {
public:
    _XCLASS_NAME(VulkanBuffer);

    explicit VulkanBuffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties
    );
    
    ~VulkanBuffer() noexcept;
    
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    VkBuffer handle() const noexcept;
    VkDeviceSize size() const noexcept;

    void realloc(    
        VkPhysicalDevice physical_device,
        VkDevice device,
        VkDeviceSize size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties
    );

    void realloc(    
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties
    );

    void realloc(    
        VkDeviceSize size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties
    );

    void ensure_capacity(VkDeviceSize size_bytes);

    void fill(VulkanCommandBuffer& command_buffer, uint32_t data);
    void fill(VulkanCommandBuffer& command_buffer, uint32_t data, VkDeviceSize size_bytes, VkDeviceSize offset = 0);
    
    VulkanBuffer& fill(
        VulkanCommandBuffer& command_buffer,
        PassWriter& fill_pass_writer,
        const void* prefab,
        uint32_t prifab_size_bytes,
        uint32_t fillable_area_size_bytes,
        uint32_t fillable_area_offset = 0u,
        uint32_t invocation_stride = 4u
    ) &;

    VulkanBuffer&& fill(
        VulkanCommandBuffer& command_buffer,
        PassWriter& fill_pass_writer,
        const void* prefab,
        uint32_t prifab_size_bytes,
        uint32_t fillable_area_size_bytes,
        uint32_t fillable_area_offset = 0u,
        uint32_t invocation_stride = 4u
    ) &&;

    template <class T>
    VulkanBuffer& fill(
        VulkanCommandBuffer& command_buffer,
        PassWriter& fill_pass_writer,
        const T& prefab,
        uint32_t fillable_area_size_bytes,
        uint32_t fillable_area_offset = 0u,
        uint32_t invocation_stride = 4u) &
    {
        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        
        fill(
            command_buffer,
            fill_pass_writer,
            &prefab,
            sizeof(prefab),
            fillable_area_size_bytes,
            fillable_area_offset,
            invocation_stride
        );

        return *this;
    }

    template <class T>
    VulkanBuffer&& fill(
        VulkanCommandBuffer& command_buffer,
        PassWriter& fill_pass_writer,
        const T& prefab,
        uint32_t fillable_area_size_bytes,
        uint32_t fillable_area_offset = 0u,
        uint32_t invocation_stride = 4u) &&
    {
        // Хотя он и так будет lvalue, но для понятности сделаю static_cast
        static_cast<VulkanBuffer&>(*this).fill(
            command_buffer,
            fill_pass_writer,
            prefab,
            fillable_area_size_bytes,
            fillable_area_offset,
            invocation_stride
        );
        
        return std::move(*this);
    }

    void upload(const void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes = 0);

    template<class T>
    inline void upload(std::span<T> data, VkDeviceSize offset_bytes = 0) {
        using Elem = std::remove_cv_t<T>;

        static_assert(std::is_trivially_copyable_v<Elem>);

        upload(
            data.data(),
            static_cast<VkDeviceSize>(sizeof(Elem) * data.size()),
            offset_bytes
        );
    }

    template<class T>
    inline void upload(const std::vector<T>& data, VkDeviceSize offset_bytes = 0) {
        upload(std::span<const T>(data), offset_bytes);
    }

    template<class T>
    inline void upload_scalar(const T& data, VkDeviceSize offset_bytes = 0) {
        static_assert(std::is_trivially_copyable_v<T>, "Type T must be trivially copyable");
        upload(&data, static_cast<VkDeviceSize>(sizeof(T)), offset_bytes);
    }

    void read(void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes);

    
    template<class T>
    inline void read(std::span<T> data, VkDeviceSize offset_bytes = 0) {
        using Elem = std::remove_cv_t<T>;

        static_assert(std::is_trivially_copyable_v<Elem>);

        read(
            data.data(),
            static_cast<VkDeviceSize>(sizeof(Elem) * data.size()),
            offset_bytes
        );
    }

    template<class T>
    inline void read(std::vector<T>& data, VkDeviceSize offset_bytes = 0) {
        static_assert(std::is_trivially_copyable_v<T>);

        read(std::span<T>(data), offset_bytes);
    }
    
    template<class T>
    inline std::vector<T> read_vector(size_t element_count, VkDeviceSize offset_bytes = 0) {
        static_assert(std::is_trivially_copyable_v<T>);

        std::vector<T> data(element_count);
        read(std::span<T>(data), offset_bytes);
        return data;
    }

    bool has_usage(VkBufferUsageFlags usage) const noexcept;
    bool has_memory_property(VkMemoryPropertyFlags properties) const noexcept;

    void memory_barrier(
        VulkanCommandBuffer& command_buffer,
        VkPipelineStageFlags src_stage,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags dst_access,
        VkDeviceSize offset_bytes = 0,
        VkDeviceSize size_bytes = VK_WHOLE_SIZE
    ) const;

    void memory_barrier_compute_write_to_compute_write_read(VulkanCommandBuffer& command_buffer) const;

    void transfer_write_to_vertex_read_barrier(
        VulkanCommandBuffer& command_buffer,
        VkDeviceSize offset_bytes = 0,
        VkDeviceSize size_bytes = VK_WHOLE_SIZE
    ) const;

    void transfer_write_to_compute_read_write_barrier(
        VulkanCommandBuffer& command_buffer,
        VkDeviceSize offset_bytes = 0,
        VkDeviceSize size_bytes = VK_WHOLE_SIZE
    ) const;

    void compute_write_to_fragment_read_barrier(
        VulkanCommandBuffer& command_buffer,
        VkDeviceSize offset_bytes = 0,
        VkDeviceSize size_bytes = VK_WHOLE_SIZE
    ) const;

    void copy_to(
        VulkanCommandBuffer& command_buffer,
        VulkanBuffer& dst_buffer,
        VkDeviceSize size_bytes,
        VkDeviceSize src_offset_bytes = 0,
        VkDeviceSize dst_offset_bytes = 0
    ) const;

    void bind_as_vertex_buffer(
        VulkanCommandBuffer& command_buffer,
        uint32_t buffer_binding = 0,
        VkDeviceSize offset = 0
    ) const;

    void bind_as_index_buffer(
        VulkanCommandBuffer& command_buffer,
        VkDeviceSize offset = 0,
        VkIndexType index_type = VK_INDEX_TYPE_UINT32
    ) const;

    static VulkanBuffer create_vertex_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_vertex_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_staging_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_staging_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_index_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_index_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_storage_index_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_storage_index_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_storage_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_storage_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_uniform_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_uniform_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_storage_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_storage_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_indirect_storage_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_indirect_storage_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_transfer_dst_storage_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_transfer_dst_storage_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_vertex_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_vertex_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_storage_vertex_buffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize size_bytes
    );

    static VulkanBuffer create_host_visible_storage_vertex_buffer(
        const VulkanEngine& engine,
        VkDeviceSize size_bytes
    );

    /*
        Функция внутри себя не ставит memory_barrier, его нужно ставить руками!
        Не забывайте!!!
    */
    template <class T>
    static VulkanBuffer from_fill(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanCommandBuffer& command_buffer,
        PassWriter& fill_pass_writer,
        VkDeviceSize buffer_size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties,
        T prefab,
        uint32_t fillable_area_size_bytes,
        uint32_t fillable_area_offset = 0,
        uint32_t invocation_stride = 4u)
    {
        LOG_NAMED("VulkanBuffer");

        static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
        
        logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
        logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
        logger.check(buffer_size_bytes != 0, "Attempt to create a buffer with zero size");

        VulkanBuffer buffer(physical_device, device, buffer_size_bytes, usage, memory_properties);
        
        buffer.fill(
            command_buffer,
            fill_pass_writer,
            prefab,
            fillable_area_size_bytes,
            fillable_area_offset,
            invocation_stride
        );

        return buffer;
    }

    VulkanBufferView get_view() noexcept;
        
private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    std::optional<VulkanMemory> m_memory = std::nullopt;
    VkDeviceSize m_size = 0;
    VkBufferUsageFlags m_usage = 0;

private:
    void destroy_buffer() noexcept;
    void set_to_default_fields(bool except_memory = false) noexcept;
    void destroy() noexcept;
};
