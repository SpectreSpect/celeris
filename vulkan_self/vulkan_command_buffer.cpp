#include "vulkan_command_buffer.h"

#include "vulkan_device.h"
#include "vulkan_command_pool.h"

#include <utility>

CommandBufferScope::CommandBufferScope(VulkanCommandBuffer& command_buffer)
    :   m_command_buffer(command_buffer) 
{
    LOG_METHOD();
    logger.check(m_command_buffer.handle() != VK_NULL_HANDLE, "Command buffer has not been initialized");

    m_command_buffer.begin();
}

CommandBufferScope::~CommandBufferScope() noexcept {
    m_command_buffer.end_noexcept();
}

VulkanCommandBuffer::VulkanCommandBuffer(
    const VulkanDevice& device,
    const VulkanCommandPool& command_pool,
    VkCommandBuffer command_buffer_handle)
    :   m_command_buffer(command_buffer_handle) ,
        m_device(device.handle()),
        m_command_pool(command_pool.handle()) {}

VulkanCommandBuffer::VulkanCommandBuffer(const VulkanDevice& device, const VulkanCommandPool& command_pool)
    :   VulkanCommandBuffer(
            std::move(VulkanCommandBuffer::create_command_buffers(device, command_pool, 1)[0])
        ) {}

VulkanCommandBuffer::~VulkanCommandBuffer() noexcept {
    destroy();
}

void VulkanCommandBuffer::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE &&
        m_command_pool != VK_NULL_HANDLE &&
        m_command_buffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(
            m_device,
            m_command_pool,
            1,
            &m_command_buffer
        );
    }

    m_device = VK_NULL_HANDLE;
    m_command_pool = VK_NULL_HANDLE;
    m_command_buffer = VK_NULL_HANDLE;
}

VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer&& other) noexcept 
    :   m_command_buffer(std::exchange(other.m_command_buffer, VK_NULL_HANDLE)), 
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_command_pool(std::exchange(other.m_command_pool, VK_NULL_HANDLE)) {}

VulkanCommandBuffer& VulkanCommandBuffer::operator=(VulkanCommandBuffer&& other) noexcept {
    if (this != &other) {
        destroy();

        m_command_buffer = std::exchange(other.m_command_buffer, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_command_pool = std::exchange(other.m_command_pool, VK_NULL_HANDLE);
    }

    return *this;
}

const VkCommandBuffer& VulkanCommandBuffer::handle() const noexcept {
    return m_command_buffer;
}

void VulkanCommandBuffer::begin() {
    LOG_METHOD();

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult begin_result = vkBeginCommandBuffer(m_command_buffer, &begin_info);

    logger.check(begin_result == VK_SUCCESS, "Failed to begin recording command buffer");
}

VkResult VulkanCommandBuffer::end_noexcept() noexcept {
    return vkEndCommandBuffer(m_command_buffer);
}

void VulkanCommandBuffer::end() {
    LOG_METHOD();
    VkResult end_result = end_noexcept();
    logger.check(end_result == VK_SUCCESS, "Failed to record command buffer");
}

CommandBufferScope VulkanCommandBuffer::begin_scope() {
    return CommandBufferScope(*this);
}

std::vector<VulkanCommandBuffer> VulkanCommandBuffer::create_command_buffers(
    const VulkanDevice& device,
    const VulkanCommandPool& command_pool,
    uint32_t count_buffers) 
{
    LOG_NAMED("VulkanCommandBuffer");

    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");

    std::vector<VulkanCommandBuffer> command_buffers;
    command_buffers.reserve(count_buffers);
    std::vector<VkCommandBuffer> command_buffer_handles(count_buffers);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool.handle();
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = count_buffers;

    VkResult result = vkAllocateCommandBuffers(
        device.handle(),
        &alloc_info,
        command_buffer_handles.data()
    );

    logger.check(result == VK_SUCCESS, "Failed to allocate command buffers");

    for (VkCommandBuffer handle : command_buffer_handles) {
        command_buffers.emplace_back(device, command_pool, handle);
    }

    return command_buffers;
}

void VulkanCommandBuffer::reset() {
    LOG_METHOD();
    VkResult result = vkResetCommandBuffer(m_command_buffer, 0);
    logger.check(result == VK_SUCCESS, "Failed to reset command buffer");
}
