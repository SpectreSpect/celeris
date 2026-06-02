#pragma once

#include <array>
#include <vector>
#include <utility>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_instance.h"
#include "vulkan_surface.h"
#include "vulkan_queue_types.h"

class VulkanPhysicalDevice {
public:
    _XCLASS_NAME(VulkanPhysicalDevice);

    static constexpr std::array<const char*, 2> device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME
    };

    explicit VulkanPhysicalDevice(
        const VulkanInstance& instance,
        const VulkanSurface& surface,
        const QueueRequest& queue_request
    );

    VulkanPhysicalDevice(const VulkanPhysicalDevice&) = delete;
    VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice&) = delete;

/*
    В теории класc не владеет никаким ресурсом, поэтому можно было бы как будто поставить = default,
    но в таком случае дескрипторы не будут зануляться, что иногда бывает неприятно.
*/
    VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept;
    VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&& other) noexcept;

    VkPhysicalDevice handle() const noexcept;
    const QueueAllocation& queue_allocation() const noexcept;

public:
    struct SwapchainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities{};
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> present_modes;
    };

public:
    bool check_device_extension_support() const;
    SwapchainSupportDetails query_swapchain_support() const;

private:
    bool find_queue_families(
        VkPhysicalDevice device,
        const QueueRequest& queue_request,
        QueueAllocation* query_allocattion
    ) const;

    bool check_device_extension_support(VkPhysicalDevice device) const;
    SwapchainSupportDetails query_swapchain_support(VkPhysicalDevice device) const;
    bool is_device_suitable(VkPhysicalDevice device, const QueueRequest& queue_request, QueueAllocation* queue_allocation) const;

    QueueAllocation m_queue_allocation{};
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
};
