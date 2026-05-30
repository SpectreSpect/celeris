#pragma once

#include <span>

#include <vulkan/vulkan.h>
#include "../logger/logger_header.h"

class VulkanDevice;
class DescriptorPoolBuilder;
class DescriptorSet;
class DescriptorSetLayout;

class DescriptorPool {
public:
    _XCLASS_NAME(DescriptorPool);

    explicit DescriptorPool(
        const VulkanDevice& device,
        std::span<const VkDescriptorPoolSize> pool_sizes,
        uint32_t max_sets,
        VkDescriptorPoolCreateFlags flags = 0
    );
    explicit DescriptorPool(const VulkanDevice& device, const DescriptorPoolBuilder& builder);

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;

    DescriptorPool(DescriptorPool&& other) noexcept;
    DescriptorPool& operator=(DescriptorPool&& other) noexcept;

    ~DescriptorPool() noexcept;

private:
    void destory() noexcept;

public:

    VkDescriptorPool handle() const noexcept;
    std::vector<DescriptorSet> allocate_sets(const DescriptorSetLayout& layout, uint32_t set_count);
    DescriptorSet allocate_set(const DescriptorSetLayout& layout);
    
private:
    VkDescriptorPool m_pool = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
};