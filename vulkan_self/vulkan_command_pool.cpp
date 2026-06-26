#include "vulkan_command_pool.h"

#include "vulkan_device.h"
#include "vulkan_queue.h"

#include <utility>

VulkanCommandPool::VulkanCommandPool(const VulkanDevice& device, QueueFamilyInfo queue_family_info) 
    :   m_device(device.handle()), 
        m_queue_family_info(queue_family_info) 
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Пока пусть будет захардкожено...
    pool_info.queueFamilyIndex = m_queue_family_info.queue_family_index;

    VkResult result = vkCreateCommandPool(
        m_device,
        &pool_info,
        nullptr,
        &m_command_pool
    );

    logger.check(result == VK_SUCCESS, "Failed to create command pool");
}

VulkanCommandPool::VulkanCommandPool(const VulkanDevice& device, const VulkanQueue& queue)
    :   VulkanCommandPool(device, queue.family_info()) {}

VulkanCommandPool::~VulkanCommandPool() noexcept {
    destroy();
}

void VulkanCommandPool::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_command_pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_command_pool = VK_NULL_HANDLE;
}

VulkanCommandPool::VulkanCommandPool(VulkanCommandPool&& other) noexcept
    :   m_command_pool(std::exchange(other.m_command_pool, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) ,
        m_queue_family_info(std::exchange(other.m_queue_family_info, QueueFamilyInfo{})) {}

VulkanCommandPool& VulkanCommandPool::operator=(VulkanCommandPool&& other) noexcept {
    if (this != &other) {
        destroy();

        m_command_pool = std::exchange(other.m_command_pool, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_queue_family_info = std::exchange(other.m_queue_family_info, QueueFamilyInfo{});
    }

    return *this;
}

VkCommandPool VulkanCommandPool::handle() const noexcept {
    return m_command_pool;
}

QueueFamilyInfo VulkanCommandPool::queue_family_info() const noexcept {
    return m_queue_family_info;
}
