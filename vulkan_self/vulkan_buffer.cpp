#include "vulkan_buffer.h"

#include <utility>

#include "vulkan_physical_device.h"
#include "vulkan_device.h"

VulkanBuffer::VulkanBuffer(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkDeviceSize buffer_size_bytes,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_properties)
        :   m_device(device.handle()),
            m_size(buffer_size_bytes),
            m_memory_properties(memory_properties)
{
    LOG_METHOD();

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_size != 0, "Attempt to create a buffer with zero size");

    try {
        m_buffer = create_buffer(device, buffer_size_bytes, usage);
        m_memory = allocate_memory(physical_device, device, m_buffer, memory_properties);

        VkResult result = vkBindBufferMemory(
            m_device,
            m_buffer,
            m_memory,
            0
        );

        logger.check(result == VK_SUCCESS, "Failed to bind buffer memory");
    } catch (...) {
        destroy();
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

    if (m_device != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(
            m_device,
            m_memory,
            nullptr
        );

        m_memory = VK_NULL_HANDLE;
    }

    m_device = VK_NULL_HANDLE;
    m_buffer = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_size = 0;
    m_memory_properties = 0;
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    :   m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
        m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE)),
        m_size(std::exchange(other.m_size, 0)),
        m_memory_properties(std::exchange(other.m_memory_properties, 0)) {}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
    if (this != &other) {
        destroy();

        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
        m_size = std::exchange(other.m_size, 0);
        m_memory_properties = std::exchange(other.m_memory_properties, 0);
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

    logger.check(data != nullptr, "Attempt to load data pointing to nullptr");
    logger.check(offset_bytes + size_bytes <= m_size);
    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_memory != VK_NULL_HANDLE, "Buffer memory is not initialized");

    logger.check(
        (m_memory_properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0,
        "Attempt to upload to non-host-visible buffer memory"
    );

    void* mapped_data = nullptr;

    VkResult result = vkMapMemory(
        m_device,
        m_memory,
        0,
        m_size,
        0,
        &mapped_data
    );

    logger.check(result == VK_SUCCESS, "Failed to map buffer memory");

    std::memcpy(
        static_cast<std::byte*>(mapped_data) + offset_bytes,
        data,
        static_cast<size_t>(size_bytes)
    );

    // Также здесь стоило бы проверять coherent и делать flush, но, опять же, позже... #TODO

    vkUnmapMemory(
        m_device,
        m_memory
    );
}

VkBuffer VulkanBuffer::create_buffer(
    const VulkanDevice& device,
    VkDeviceSize buffer_size_bytes,
    VkBufferUsageFlags usage) 
{
    LOG_NAMED("VulkanBuffer");

    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");

    VkBuffer buffer_handle = VK_NULL_HANDLE;

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = buffer_size_bytes;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkResult result = vkCreateBuffer(
        device.handle(),
        &buffer_info,
        nullptr,
        &buffer_handle
    );

    logger.check(result == VK_SUCCESS, "Failed to create buffer");

    return buffer_handle;
}

uint32_t VulkanBuffer::find_memory_type(
    const VulkanPhysicalDevice& physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags properties)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(
        physical_device.handle(),
        &memory_properties
    );

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        bool type_supported = type_filter & (1u << i);

        bool has_required_properties =
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties;

        if (type_supported && has_required_properties) {
            return i;
        }
    }

    logger.throw_error("Failed to find suitable memory type");
    return 0;
}

VkDeviceMemory VulkanBuffer::allocate_memory(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkBuffer buffer,
    VkMemoryPropertyFlags memory_properties)
{
    LOG_NAMED("VulkanBuffer");

    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(buffer != VK_NULL_HANDLE, "Buffer is not initialized");

    VkDeviceMemory memory = VK_NULL_HANDLE;

    VkMemoryRequirements memory_requirements{};
    vkGetBufferMemoryRequirements(
        device.handle(),
        buffer,
        &memory_requirements
    );

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(
        physical_device,
        memory_requirements.memoryTypeBits,
        memory_properties
    );

    VkResult result = vkAllocateMemory(
        device.handle(),
        &alloc_info,
        nullptr,
        &memory
    );

    logger.check(result == VK_SUCCESS, "Failed to allocate buffer memory");

    return memory;
}
