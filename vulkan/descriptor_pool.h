#pragma once
#include "descriptor_set_layout.h"
#include "vulkan_utils.h"
#include <vulkan/vulkan.h>
#include <unordered_map>


class DescriptorPool {
public:
    VkDescriptorPool pool;

    DescriptorPool() = default;
    void create(VkDevice& device, DescriptorSetLayout& layout, uint32_t num_sets = 1);
};