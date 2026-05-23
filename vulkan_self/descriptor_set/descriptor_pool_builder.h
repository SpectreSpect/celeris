#pragma once

#include <vector>
#include <span>
#include <vulkan/vulkan.h>

#include "../logger/logger_header.h"

class DescriptorSetLayoutBuilder;

class DescriptorPoolBuilder {
public:
    _XCLASS_NAME(DescriptorPoolBuilder);

    DescriptorPoolBuilder() = default;
    
    DescriptorPoolBuilder& add_descriptors(VkDescriptorType type, uint32_t descriptor_count);
    DescriptorPoolBuilder& add_layout(const DescriptorSetLayoutBuilder& layout_builder, uint32_t set_count = 1);

    std::span<const VkDescriptorPoolSize> pool_sizes() const noexcept;
    uint32_t max_sets() const noexcept;
    void set_max_sets(uint32_t max_sets) noexcept;
private:
    std::vector<VkDescriptorPoolSize> m_pool_sizes;
    uint32_t m_max_sets = 0;
};