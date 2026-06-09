#include "vulkan_semaphore.h"

#include <utility>

#include "vulkan_device.h"



VulkanSemaphore::VulkanSemaphore(const VulkanDevice& device) : m_device(device.handle()) {
    LOG_METHOD();

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkResult result = vkCreateSemaphore(
        m_device,
        &semaphore_info,
        nullptr,
        &m_semaphore
    );

    logger.check(
        result == VK_SUCCESS,
        "Failed to create semaphore"
    );
}
    
VulkanSemaphore::~VulkanSemaphore() noexcept {
    destroy();
}

void VulkanSemaphore::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device, m_semaphore, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_semaphore = VK_NULL_HANDLE;
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept
    :   m_semaphore(std::exchange(other.m_semaphore, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept {
    if (this != &other) {
        destroy();

        m_semaphore = std::exchange(other.m_semaphore, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkSemaphore VulkanSemaphore::handle() const noexcept {
    return m_semaphore;
}

std::vector<VulkanSemaphore> VulkanSemaphore::create_semaphores(const VulkanDevice& device, size_t count_semaphores) {
    LOG_NAMED("VulkanSemaphore");

    std::vector<VulkanSemaphore> semaphores;
    semaphores.reserve(count_semaphores);

    for (size_t i = 0; i < count_semaphores; i++) {
        semaphores.emplace_back(device);
    }

    return semaphores;
}
