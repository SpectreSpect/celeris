#pragma once

#include "../vulkan_self/logger/logger_header.h"

#include "../vulkan_self/vulkan_buffer.h"

class InstanceBatch {
public:
    _XCLASS_NAME(InstanceBatch);

    InstanceBatch();

    VulkanBuffer buffer;
    uint32_t instance_count;
};