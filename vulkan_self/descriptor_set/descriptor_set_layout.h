#pragma once
#include <vulkan/vulkan.h>
#include "../logger/logger_header.h"

class VulkanDevice;

class DescriptorSetLayout {
public:
    _XCLASS_NAME(DescriptorSetLayout);

    explicit DescriptorSetLayout(const VulkanDevice& device, uint32_t binding_count, const VkDescriptorSetLayoutBinding* bindings);

    DescriptorSetLayout(const DescriptorSetLayout&) = delete;
    DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;

    VkDescriptorSetLayout handle() const noexcept;
private:
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
};