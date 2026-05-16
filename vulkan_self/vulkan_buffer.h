#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <cstring>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanCommandBuffer;

class VulkanBuffer {
public:
    _XCLASS_NAME(VulkanBuffer);

    explicit VulkanBuffer(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkDeviceSize buffer_size_bytes,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags memory_properties
    );
    
    ~VulkanBuffer() noexcept;
    void destroy() noexcept;

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;

    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    const VkBuffer& handle() const noexcept;
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

    void bind_as_vertex_buffer(
        VulkanCommandBuffer& command_buffer,
        uint32_t buffer_binding = 0,
        VkDeviceSize offset = 0
    ) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDeviceSize m_size = 0;
    VkMemoryPropertyFlags m_memory_properties = 0;

private:
    static VkBuffer create_buffer(
        const VulkanDevice& device,
        VkDeviceSize buffer_size_bytes,
        VkBufferUsageFlags usage
    );

    static uint32_t find_memory_type(
        const VulkanPhysicalDevice& physical_device,
        uint32_t type_filter,
        VkMemoryPropertyFlags properties
    );

    static VkDeviceMemory allocate_memory(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkBuffer buffer,
        VkMemoryPropertyFlags memory_properties
    );
};
