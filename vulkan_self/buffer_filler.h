#pragma once

#include <type_traits>
#include <vulkan/vulkan.h>

#include "logger/logger_header.h"
#include "vulkan_buffer.h"
#include "../renderer/compute_pass_instance.h"

class ComputePassManager;
class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanCommandBuffer;

class BufferFiller {
public:
    _XCLASS_NAME(BufferFiller);

    BufferFiller(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        ComputePassManager& compute_pass_manager, 
        VkDeviceSize initial_prifab_buffer_size
    );

    ~BufferFiller() noexcept = default;

    BufferFiller(const BufferFiller&) = delete;
    BufferFiller& operator=(const BufferFiller&) = delete;

    BufferFiller(BufferFiller&&) noexcept = default;
    BufferFiller& operator=(BufferFiller&&) noexcept = default;

    VulkanBuffer& fill_buffer(
        VulkanCommandBuffer& command_buffer,
        const void* data,
        uint32_t size_data_bytes,
        VulkanBuffer& dst_buffer,
        uint32_t offset_in_dst = 0u,
        uint32_t invocation_stride = 0u
    );

    template <class T>
    VulkanBuffer& fill_buffer(
        VulkanCommandBuffer& command_buffer,
        T fill_value,
        VulkanBuffer& dst_buffer,
        uint32_t offset_in_dst = 0u,
        uint32_t invocation_stride = 0u)
    {
        LOG_METHOD();

        static_assert(std::is_trivially_copyable<T>, "T must be trivially copyable");

        fill_buffer(
            command_buffer,
            &fill_value,
            sizeof(T),
            dst_buffer,
            offset_in_dst,
            invocation_stride
        );

        return dst_buffer;
    }

private:
    VulkanBuffer m_prifab_buffer;
    ComputePassInstance m_fill_pass_instance;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};
