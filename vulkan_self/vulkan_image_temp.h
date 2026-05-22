#pragma once

#include <vector>
#include <cstdint>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;
class VulkanPhysicalDevice;

class VulkanImageTemp {
public:
    _XCLASS_NAME(VulkanImageTemp);

    VulkanImageTemp(
        const VkImageCreateInfo& create_info,
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkMemoryPropertyFlags memory_properties
    );

    ~VulkanImageTemp();

    VulkanImageTemp(const VulkanImageTemp&) = delete;
    VulkanImageTemp& operator=(const VulkanImageTemp&) = delete;

    VulkanImageTemp(VulkanImageTemp&& other) noexcept;
    VulkanImageTemp& operator=(VulkanImageTemp&& other) noexcept;

    void destroy();

    VkImage handle() const noexcept;
    VkDeviceMemory memory() const noexcept;
    VkFormat format() const noexcept;
    VkExtent3D extent() const noexcept;

    static VkImageCreateInfo create_depth_desc(
        VkExtent2D extent,
        VkFormat format
    );

    static VulkanImageTemp create_depth_image(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent,
        VkFormat format
    );

    static std::vector<VulkanImageTemp> create_depth_images(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VkExtent2D extent,
        VkFormat format,
        size_t count
    );

private:
    VkImage m_image = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkFormat m_format = VK_FORMAT_UNDEFINED;
    VkExtent3D m_extent{};

private:
    static uint32_t find_memory_type(
        VkPhysicalDevice physical_device,
        uint32_t type_filter,
        VkMemoryPropertyFlags properties
    );
};