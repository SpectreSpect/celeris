#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <type_traits>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanBuffer;

class VulkanMemory {
public:
    _XCLASS_NAME(VulkanMemory);

    explicit VulkanMemory(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        uint32_t memory_type_index,
        VkDeviceSize size_bytes
    );
    
    explicit VulkanMemory(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        uint32_t memory_type_filter,
        VkMemoryPropertyFlags required_properties,
        VkDeviceSize size_bytes
    );

    explicit VulkanMemory(
        VkPhysicalDevice physical_device,
        VkDevice device,
        uint32_t memory_type_filter,
        VkMemoryPropertyFlags required_properties,
        VkDeviceSize size_bytes
    );
    
    ~VulkanMemory() noexcept;

public:
    VulkanMemory(const VulkanMemory&) = delete;
    VulkanMemory& operator=(const VulkanMemory&) = delete;
    
    VulkanMemory(VulkanMemory&& other) noexcept;
    VulkanMemory& operator=(VulkanMemory&& other) noexcept;

    VkDeviceMemory handle() const noexcept;
    VkDeviceSize size() const noexcept;
    uint32_t memory_type_index() const noexcept;
    VkMemoryPropertyFlags properties() const noexcept;
    bool is_host_visible() const noexcept;
    bool is_host_coherent() const noexcept;

    void map_memory(
        void*& mapped_memory,
        VkDeviceSize size_bytes,
        VkDeviceSize offset_bytes = 0,
        VkMemoryMapFlags map_flags = 0
    );

    void map_memory(void*& mapped_memory, VkMemoryMapFlags map_flags = 0);
    void unmap_memory();

    void flush(VkDeviceSize size_bytes, VkDeviceSize offset_bytes = 0) const;
    void flush() const;

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

    void invalidate(VkDeviceSize size_bytes, VkDeviceSize offset_bytes = 0) const;
    void invalidate() const;

    void read(void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes = 0);

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

    void bind_to_buffer(VulkanBuffer& buffer, VkDeviceSize memory_offset = 0) const;
    void bind_to_image(VkImage image, VkDeviceSize memory_offset = 0) const;

private:
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkDeviceSize m_size = 0;
    uint32_t m_memory_type_index = 0;
    VkMemoryPropertyFlags m_memory_properties = 0;
    VkDeviceSize m_non_coherent_atom_size = 1;

private:
    void destroy() noexcept;

    void allocate(
        VkPhysicalDevice physical_device,
        VkDevice device,
        uint32_t memory_type_index,
        VkDeviceSize size_bytes
    );

    void allocate(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        uint32_t memory_type_index,
        VkDeviceSize size_bytes
    );

    static uint32_t find_memory_type(
        VkPhysicalDevice physical_device,
        uint32_t type_filter,
        VkMemoryPropertyFlags properties
    );

    static uint32_t find_memory_type(
        const VulkanPhysicalDevice& physical_device,
        uint32_t type_filter,
        VkMemoryPropertyFlags properties
    );

    static VkMemoryPropertyFlags get_memory_type_properties(
        VkPhysicalDevice physical_device,
        uint32_t memory_type_index
    );

    static VkMemoryPropertyFlags get_memory_type_properties(
        const VulkanPhysicalDevice& physical_device,
        uint32_t memory_type_index
    );
};
