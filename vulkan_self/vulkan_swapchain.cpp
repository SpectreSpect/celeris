#include "vulkan_swapchain.h"
#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "vulkan_surface.h"
#include "window.h"
#include "vulkan_fence.h"
#include "vulkan_semaphore.h"

#include <limits>
#include <utility>
#include <algorithm>

VulkanSwapchain::VulkanSwapchain(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    const VulkanSurface& surface,
    const Window& window) : m_device(device.handle())
{
    LOG_METHOD();

    auto swapchain_support = physical_device.query_swapchain_support();
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swapchain_support.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support.present_modes);
    VkExtent2D extent = choose_swap_extent(swapchain_support.capabilities, window);

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

    if (swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface.handle();

    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const VulkanQueue& graphics_queue = device.graphics_queue();
    const VulkanQueue& present_queue = device.present_queue();

    uint32_t queue_family_indices[] = {
        graphics_queue.location().family_index,
        present_queue.location().family_index
    };

    if (graphics_queue.location().family_index != present_queue.location().family_index) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(device.handle(), &create_info, nullptr, &m_swapchain);
    logger.check(result == VK_SUCCESS, "Failed to create swapchain");

    vkGetSwapchainImagesKHR(device.handle(), m_swapchain, &image_count, nullptr);
    m_images.resize(image_count);
    vkGetSwapchainImagesKHR(
        device.handle(),
        m_swapchain,
        &image_count,
        m_images.data()
    );

    m_image_format = surface_format.format;
    m_extent = extent;
}

VulkanSwapchain::~VulkanSwapchain() {
    destroy();
}

void VulkanSwapchain::destroy() {
    if (m_swapchain != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    }

    m_swapchain = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;

    m_images.clear();

    m_image_format = VK_FORMAT_UNDEFINED;
    m_extent = {};
}

VulkanSwapchain::VulkanSwapchain(VulkanSwapchain&& other) noexcept 
    :   m_swapchain(std::exchange(other.m_swapchain, VK_NULL_HANDLE)),
        m_images(std::move(other.m_images)),
        m_image_format(std::exchange(other.m_image_format, VK_FORMAT_UNDEFINED)),
        m_extent(std::exchange(other.m_extent, VkExtent2D{})),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanSwapchain& VulkanSwapchain::operator=(VulkanSwapchain&& other) noexcept {
    if (this != &other) {
        destroy();

        m_swapchain = std::exchange(other.m_swapchain, VK_NULL_HANDLE);
        m_images = std::move(other.m_images);
        m_image_format = std::exchange(other.m_image_format, VK_FORMAT_UNDEFINED);
        m_extent = std::exchange(other.m_extent, VkExtent2D{});
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

const VkSwapchainKHR& VulkanSwapchain::handle() const noexcept {
    return m_swapchain;
}

VkImage VulkanSwapchain::image(uint32_t index) const {
    LOG_METHOD();
    
    logger.check(!m_images.empty(), "There is no available swapchain images");
    logger.check(index < m_images.size(), "Index is out of bound of the array");

    return m_images[index];
}

const std::vector<VkImage>& VulkanSwapchain::images() const noexcept {
    return m_images;
}

VkFormat VulkanSwapchain::image_format() const noexcept {
    return m_image_format;
}

VkExtent2D VulkanSwapchain::extent() const noexcept {
    return m_extent;
}

VkResult VulkanSwapchain::acquire_next_image(
    uint32_t& image_index_out,
    VulkanSemaphore& image_available_semaphore,
    VulkanFence* image_available_fence,
    uint64_t timeout)
{
    LOG_METHOD();
    VkResult acquire_result = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        timeout,
        image_available_semaphore.handle(),
        image_available_fence != nullptr ? image_available_fence->handle() : VK_NULL_HANDLE,
        &image_index_out
    );

    logger.check(
        acquire_result == VK_SUCCESS ||
        acquire_result == VK_SUBOPTIMAL_KHR ||
        acquire_result == VK_ERROR_OUT_OF_DATE_KHR,
        "Failed to acquire swapchain image"
    );

    return acquire_result;
}

VkSurfaceFormatKHR VulkanSwapchain::choose_swap_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats) const 
{
    LOG_METHOD();

    logger.check(!available_formats.empty(), "No available swapchain surface formats");

    for (const auto& available_format : available_formats) {
        
        // if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
        if (available_format.format == VK_FORMAT_B8G8R8A8_UNORM &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR VulkanSwapchain::choose_swap_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes) const 
{
    LOG_METHOD();

    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::choose_swap_extent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    const Window& window) const 
{
    LOG_METHOD();

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window.handle(), &width, &height);

    VkExtent2D actual_extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actual_extent.width = std::clamp(
        actual_extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
    );

    actual_extent.height = std::clamp(
        actual_extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
    );

    return actual_extent;
}
