#include "vulkan_buffer.h"

#include <utility>

#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "vulkan_command_buffer.h"

VulkanBuffer::VulkanBuffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties)
        :   m_device(device.handle()),
            m_size(size_bytes),
            m_usage(usage)
{
    LOG_METHOD();

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(size_bytes != 0, "Attempt to create a buffer with zero size");

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size_bytes;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(
        m_device,
        &buffer_info,
        nullptr,
        &m_buffer
    );

    logger.check(result == VK_SUCCESS, "Failed to create buffer");

    try {
        VkMemoryRequirements memory_requirements{};
        vkGetBufferMemoryRequirements(
            m_device,
            m_buffer,
            &memory_requirements
        );

        m_memory.emplace(
            physical_device,
            device,
            memory_requirements.memoryTypeBits,
            memory_properties,
            memory_requirements.size
        );

        m_memory->bind_to_buffer(*this);
    } catch (...) {
        vkDestroyBuffer(m_device, m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
        m_device = VK_NULL_HANDLE;
        m_size = 0;
        m_usage = 0;
        throw;
    }
}

VulkanBuffer::~VulkanBuffer() noexcept {
    destroy();
}

void VulkanBuffer::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(
            m_device,
            m_buffer,
            nullptr
        );
    }

    m_buffer = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
    m_size = 0;
    m_usage = 0;

    m_memory.reset();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    :   m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
        m_memory(std::move(other.m_memory)),
        m_size(std::exchange(other.m_size, 0)),
        m_usage(std::exchange(other.m_usage, 0)) {}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
    if (this != &other) {
        destroy();

        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::move(other.m_memory);
        m_size = std::exchange(other.m_size, 0);
        m_usage = std::exchange(other.m_usage, 0);
    }

    return *this;
} 

VkBuffer VulkanBuffer::handle() const noexcept {
    return m_buffer;
}

VkDeviceSize VulkanBuffer::size() const noexcept {
    return m_size;
}

void VulkanBuffer::upload(const void* data, VkDeviceSize size_bytes, VkDeviceSize offset_bytes) {
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(m_memory.has_value(), "Buffer memory is not initialized");
    logger.check(data != nullptr, "Attempt to upload data from nullptr");
    logger.check(size_bytes != 0, "Attempt to upload zero bytes");
    logger.check(offset_bytes <= m_size, "Upload offset is out of bounds");
    logger.check(size_bytes <= m_size - offset_bytes, "Upload range is out of bounds");

    m_memory->upload(data, size_bytes, offset_bytes);
}

void VulkanBuffer::bind_as_vertex_buffer(
    VulkanCommandBuffer& command_buffer,
    uint32_t buffer_binding,
    VkDeviceSize offset) const 
{
    LOG_METHOD();

    logger.check(m_buffer != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(
        (m_usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) != 0,
        "Attempt to bind buffer as vertex buffer, but it was not created with VK_BUFFER_USAGE_VERTEX_BUFFER_BIT"
    );
    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");

    // Позже сделать возможность использовать несколько биндингов. #TODO
    vkCmdBindVertexBuffers(command_buffer.handle(), buffer_binding, 1, &m_buffer, &offset);
}
