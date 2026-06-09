#pragma once
#include <vulkan/vulkan.h>
#include "descriptor_set_bundle.h"

class Pipeline {
public:
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    DescriptorSetBundle* descriptor_set_bundle = nullptr;

    Pipeline() = default;
    virtual ~Pipeline() = default;

    virtual VkPipelineBindPoint get_bind_point() const = 0;
};