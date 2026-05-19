#pragma once

#include <vulkan/vulkan.h>
#include <span>

#include "../logger/logger_header.h"

class VulkanDevice;
class DescriptorSetLayoutBuilder;

class DescriptorSetLayout {
public:
    _XCLASS_NAME(DescriptorSetLayout);

    explicit DescriptorSetLayout(const VulkanDevice& device, std::span<const VkDescriptorSetLayoutBinding> bindings);
    explicit DescriptorSetLayout(const VulkanDevice& device, const DescriptorSetLayoutBuilder& builder);

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    VkDescriptorSetLayout handle() const noexcept;
private:
    VkDescriptorSetLayout m_layout = VK_NULL_HANDLE;
    const VkDevice m_device = VK_NULL_HANDLE;
};