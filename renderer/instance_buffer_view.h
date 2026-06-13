#pragma once

#include <cstdint>
#include "../vulkan_self/logger/logger_header.h"

class VulkanBuffer;

class InstanceBufferView {
public:
    _XCLASS_NAME(InstanceBufferView);

    InstanceBufferView() = default;
    InstanceBufferView(VulkanBuffer& buffer, uint32_t instance_count, uint32_t instance_size);

    bool valid() const noexcept;
    uint32_t instance_count() const noexcept;
    void set_instance_count(uint32_t instance_count);
    uint32_t instance_size() const noexcept;
    
    const VulkanBuffer& buffer() const;
    VulkanBuffer& buffer();

private:
    VulkanBuffer* m_buffer = nullptr;
    uint32_t m_instance_count = 0;
    uint32_t m_instance_size = 0;
};