#pragma once

#include <cstdint>

class VulkanBuffer;

class DispatchArg {
public:
    static constexpr uint32_t USE_DIRECT_VALUE = 0xFFFFFFFFu;
    static constexpr uint32_t USE_DEFAULT_WORKGROUP_SIZE = 0xFFFFFFFFu;

    const VulkanBuffer* arg_buffer;
    uint32_t offset_bytes;
    uint32_t direct_value;
    uint32_t workgroup_size;

    DispatchArg(const VulkanBuffer* arg_buffer, uint32_t offset_bytes, uint32_t direct_value, uint32_t workgroup_size = USE_DEFAULT_WORKGROUP_SIZE);
};