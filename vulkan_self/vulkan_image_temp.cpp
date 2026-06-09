#include "vulkan_image_temp.h"

#include "vulkan_device.h"
#include "vulkan_physical_device.h"

#include <utility>

VulkanImageTemp::VulkanImageTemp(
    const VkImageCreateInfo& create_info,
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkMemoryPropertyFlags memory_properties)
    :   m_device(device.handle()),
        m_format(create_info.format),
        m_extent(create_info.extent)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(physical_device.handle() != VK_NULL_HANDLE, "Physical device is not initialized");

    VkResult result = vkCreateImage(
        m_device,
        &create_info,
        nullptr,
        &m_image
    );

    logger.check(result == VK_SUCCESS, "Failed to create Vulkan image");

    VkMemoryRequirements memory_requirements{};
    vkGetImageMemoryRequirements(
        m_device,
        m_image,
        &memory_requirements
    );

    VkMemoryAllocateInfo allocate_info{};
    allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocate_info.allocationSize = memory_requirements.size;
    allocate_info.memoryTypeIndex = find_memory_type(
        physical_device.handle(),
        memory_requirements.memoryTypeBits,
        memory_properties
    );

    result = vkAllocateMemory(
        m_device,
        &allocate_info,
        nullptr,
        &m_memory
    );

    logger.check(result == VK_SUCCESS, "Failed to allocate Vulkan image memory");

    result = vkBindImageMemory(
        m_device,
        m_image,
        m_memory,
        0
    );

    logger.check(result == VK_SUCCESS, "Failed to bind Vulkan image memory");
}

VulkanImageTemp::~VulkanImageTemp() {
    destroy();
}

void VulkanImageTemp::destroy() {
    if (m_device != VK_NULL_HANDLE && m_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_image, nullptr);
    }

    if (m_device != VK_NULL_HANDLE && m_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_memory, nullptr);
    }

    m_image = VK_NULL_HANDLE;
    m_memory = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;

    m_format = VK_FORMAT_UNDEFINED;
    m_extent = {};
}

VulkanImageTemp::VulkanImageTemp(VulkanImageTemp&& other) noexcept
    :   m_image(std::exchange(other.m_image, VK_NULL_HANDLE)),
        m_memory(std::exchange(other.m_memory, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
        m_format(std::exchange(other.m_format, VK_FORMAT_UNDEFINED)),
        m_extent(std::exchange(other.m_extent, VkExtent3D{})) {}

VulkanImageTemp& VulkanImageTemp::operator=(VulkanImageTemp&& other) noexcept {
    if (this != &other) {
        destroy();

        m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
        m_memory = std::exchange(other.m_memory, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);

        m_format = std::exchange(other.m_format, VK_FORMAT_UNDEFINED);
        m_extent = std::exchange(other.m_extent, VkExtent3D{});
    }

    return *this;
}

VkImage VulkanImageTemp::handle() const noexcept {
    return m_image;
}

VkDeviceMemory VulkanImageTemp::memory() const noexcept {
    return m_memory;
}

VkFormat VulkanImageTemp::format() const noexcept {
    return m_format;
}

VkExtent3D VulkanImageTemp::extent() const noexcept {
    return m_extent;
}

uint32_t VulkanImageTemp::find_memory_type(
    VkPhysicalDevice physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags properties)
{
    LOG_NAMED("VulkanImageTemp");

    VkPhysicalDeviceMemoryProperties memory_properties{};
    vkGetPhysicalDeviceMemoryProperties(
        physical_device,
        &memory_properties
    );

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++) {
        bool type_matches = (type_filter & (1u << i)) != 0;

        bool properties_match =
            (memory_properties.memoryTypes[i].propertyFlags & properties) == properties;

        if (type_matches && properties_match) {
            return i;
        }
    }

    logger.check(false, "Failed to find suitable image memory type");
    return 0;
}

VkImageCreateInfo VulkanImageTemp::create_depth_desc(
    VkExtent2D extent,
    VkFormat format)
{
    LOG_NAMED("VulkanImageTemp");

    VkImageCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    create_info.imageType = VK_IMAGE_TYPE_2D;

    create_info.extent.width = extent.width;
    create_info.extent.height = extent.height;
    create_info.extent.depth = 1;

    create_info.mipLevels = 1;
    create_info.arrayLayers = 1;

    create_info.format = format;
    create_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    create_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    return create_info;
}

VulkanImageTemp VulkanImageTemp::create_depth_image(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent,
    VkFormat format)
{
    LOG_NAMED("VulkanImageTemp");

    return VulkanImageTemp(
        create_depth_desc(extent, format),
        physical_device,
        device,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
}

std::vector<VulkanImageTemp> VulkanImageTemp::create_depth_images(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    VkExtent2D extent,
    VkFormat format,
    size_t count)
{
    LOG_NAMED("VulkanImageTemp");

    std::vector<VulkanImageTemp> images;
    images.reserve(count);

    for (size_t i = 0; i < count; i++) {
        images.emplace_back(
            create_depth_desc(extent, format),
            physical_device,
            device,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );
    }

    return images;
}