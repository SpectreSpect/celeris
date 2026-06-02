#pragma once

#include <type_traits>
#include <vector>
#include <utility>
#include <cstdint>
#include <cstddef>
#include <vulkan/vulkan.h>

#include "logger/logger_header.h"
#include "vulkan_buffer.h"
#include "../renderer/compute_pass_instance.h"
#include "vulkan_fence.h"

class ComputePassManager;
class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanCommandBuffer;
class VulkanCommandPool;
class VulkanQueue;

class BufferFiller {
public:
    _XCLASS_NAME(BufferFiller);

    BufferFiller(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        const VulkanCommandPool& command_pool,
        VulkanQueue& queue,
        ComputePassManager& compute_pass_manager, 
        VkDeviceSize initial_prifab_buffer_size,
        uint32_t count_fills_in_flight = 10
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
        const T& fill_value,
        VulkanBuffer& dst_buffer,
        uint32_t offset_in_dst = 0u,
        uint32_t invocation_stride = 0u)
    {
        LOG_METHOD();

        static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");

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

    template <class T>
    VulkanBuffer fill_buffer(
        VulkanCommandBuffer& command_buffer,
        const T& fill_value,
        VulkanBuffer&& dst_buffer,
        uint32_t offset_in_dst = 0u,
        uint32_t invocation_stride = 0u)
    {
        fill_buffer(
            command_buffer,
            fill_value,
            dst_buffer,
            offset_in_dst,
            invocation_stride
        );

        return std::move(dst_buffer);
    }

private:
    struct FillResource {
        VulkanCommandBuffer m_command_buffer; 
        ComputePassInstance m_fill_pass_instance;
        VulkanFence m_fence;

        FillResource(
            VulkanCommandBuffer&& command_buffer, 
            ComputePassInstance&& fill_pass_instance,
            VulkanFence&& fence)
            :   m_command_buffer(std::move(command_buffer)),
                m_fill_pass_instance(std::move(fill_pass_instance)),
                m_fence(std::move(fence)) {}
    };

private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VulkanQueue* m_queue = nullptr; // Нужен стабильный адерес (в VulkanDevice обеспечено)
    std::vector<FillResource> m_fill_resources;
    VulkanBuffer m_prifab_buffer;
    size_t current_fill_resource_id = 0;

    static std::vector<FillResource> create_fill_resources(
        const VulkanDevice& device,
        const VulkanCommandPool& command_pool,
        ComputePassManager& compute_pass_manager,
        size_t count_fill_recources
    );
};
