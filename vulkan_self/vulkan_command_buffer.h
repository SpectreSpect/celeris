#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanCommandPool;
class VulkanDevice;
class VulkanCommandBuffer;
class VulkanBuffer;

class CommandBufferScope {
public:
    _XCLASS_NAME(CommandBufferScope);

    CommandBufferScope() = delete;
    CommandBufferScope(VulkanCommandBuffer& command_buffer);
    ~CommandBufferScope() noexcept;

    CommandBufferScope(const CommandBufferScope&) = delete;
    CommandBufferScope& operator=(const CommandBufferScope&) = delete;
    CommandBufferScope(CommandBufferScope&&) = delete;
    CommandBufferScope& operator=(CommandBufferScope&&) = delete;

private:
    VulkanCommandBuffer& m_command_buffer;
};

class VulkanCommandBuffer {
public:
    _XCLASS_NAME(VulkanCommandBuffer);

    explicit VulkanCommandBuffer(
        const VulkanDevice& device,
        const VulkanCommandPool& command_pool,
        VkCommandBuffer command_buffer_handle
    );
    explicit VulkanCommandBuffer(const VulkanDevice& device, const VulkanCommandPool& command_pool);
    ~VulkanCommandBuffer() noexcept;
    void destroy() noexcept;

    VulkanCommandBuffer(const VulkanCommandBuffer&) = delete;
    VulkanCommandBuffer& operator=(const VulkanCommandBuffer&) = delete;

    VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer&& other) noexcept;

    const VkCommandBuffer& handle() const noexcept;

    void begin();
    VkResult end_noexcept() noexcept;
    void end();

    CommandBufferScope begin_scope();

    void reset();

    void dispatch(uint32_t x_groups, uint32_t y_groups, uint32_t z_groups);
    void dispatch_indirect(const VulkanBuffer& args);

    void draw_indexed(uint32_t index_count, 
                      uint32_t instance_count = 1, 
                      uint32_t first_index = 0, 
                      uint32_t vertex_offset = 0, 
                      uint32_t first_instance = 0);

    static std::vector<VulkanCommandBuffer> create_command_buffers(
        const VulkanDevice& device,
        const VulkanCommandPool& command_pool,
        uint32_t count_buffers
    );

private:
    VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
};
