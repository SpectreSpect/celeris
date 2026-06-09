#include "vulkan_surface.h"

VulkanSurface::VulkanSurface(const VulkanInstance& instance, const Window& window) : m_instance(instance.handle())
{
    LOG_METHOD();
    VkResult result = glfwCreateWindowSurface(m_instance, window.handle(), nullptr, &m_surface);
    logger.check(result == VK_SUCCESS, "Failed to create window surface");
}

VulkanSurface::~VulkanSurface() {
    destroy();
}

void VulkanSurface::destroy() {
    if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    }

    m_surface = VK_NULL_HANDLE;
    m_instance = VK_NULL_HANDLE;
}

VulkanSurface::VulkanSurface(VulkanSurface&& other) noexcept
    :   m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE)),
        m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)) {}

VulkanSurface& VulkanSurface::operator=(VulkanSurface&& other) noexcept {
    if (this != &other) {
        destroy();

        m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
    }

    return *this;
}

VkSurfaceKHR VulkanSurface::handle() const noexcept {
    return m_surface;
}

VkInstance VulkanSurface::instance_handle() const noexcept {
    return m_instance;
}
