#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_memory.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanCommandBuffer;

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

private:
    void destroy() noexcept;

public:
    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    VkBuffer handle() const noexcept;
    VkDeviceSize size() const noexcept;

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
        return data;
    }
    
    template<class T>
    inline std::vector<T> read_vector(size_t element_count, VkDeviceSize offset_bytes = 0) {
        static_assert(std::is_trivially_copyable_v<T>);

        std::vector<T> data(element_count);
        read(std::span<T>(data), offset_bytes);
        return data;
    }

    bool has_usage(VkBufferUsageFlags usage) const noexcept;

    void memory_barrier(
        VulkanCommandBuffer& command_buffer,
        VkPipelineStageFlags src_stage,
        VkAccessFlags src_access,
        VkPipelineStageFlags dst_stage,
        VkAccessFlags dst_access,
        VkDeviceSize offset_bytes = 0,
        VkDeviceSize size_bytes = VK_WHOLE_SIZE
    ) const;

    void transfer_write_to_vertex_read_barrier(
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

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    std::optional<VulkanMemory> m_memory = std::nullopt;
    VkDeviceSize m_size = 0;
    VkBufferUsageFlags m_usage = 0;
};
