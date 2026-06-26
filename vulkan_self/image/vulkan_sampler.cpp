#include "vulkan_sampler.h"

#include <utility>

#include "../vulkan_device.h"

VulkanSampler::VulkanSampler(
    const VulkanDevice& device,
    const VkSamplerCreateInfo& desc)
{
    LOG_METHOD();
    create_sampler(device, desc);
}

VulkanSampler::VulkanSampler(
    const VulkanDevice& device,
    VkFilter filter,
    VkSamplerAddressMode address_mode,
    VkSamplerMipmapMode mipmap_mode)
{   
    LOG_METHOD();
    
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    sampler_info.magFilter = filter;
    sampler_info.minFilter = filter;

    sampler_info.addressModeU = address_mode;
    sampler_info.addressModeV = address_mode;
    sampler_info.addressModeW = address_mode;

    sampler_info.anisotropyEnable = VK_FALSE;
    sampler_info.maxAnisotropy = 1.0f;

    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;

    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;

    sampler_info.mipmapMode = mipmap_mode;
    sampler_info.mipLodBias = 0.0f;
    sampler_info.minLod = 0.0f;
    sampler_info.maxLod = 0.0f;

    create_sampler(device, sampler_info);
}

VulkanSampler::~VulkanSampler() noexcept {
    destroy();
}

void VulkanSampler::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(m_device, m_sampler, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_sampler = VK_NULL_HANDLE;
}

VulkanSampler::VulkanSampler(VulkanSampler&& other) noexcept
    :   m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanSampler& VulkanSampler::operator=(VulkanSampler&& other) noexcept {
    if (this != &other) {
        destroy();

        m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkSampler VulkanSampler::handle() const noexcept {
    return m_sampler;
}

void VulkanSampler::create_sampler(
    const VulkanDevice& device,
    const VkSamplerCreateInfo& desc)
{
    LOG_METHOD();

    logger.check(
        desc.sType == VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        "Invalid VkSamplerCreateInfo::sType"
    );

    logger.check(m_sampler == VK_NULL_HANDLE, "Sampler is already initialized");
    logger.check(device.handle() != VK_NULL_HANDLE, "Device is not initialized");

    m_device = device.handle();

    VkResult result = vkCreateSampler(
        m_device,
        &desc,
        nullptr,
        &m_sampler
    );

    logger.check(result == VK_SUCCESS, "Failed to create Vulkan sampler");
}
