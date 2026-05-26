#pragma once

#include <cstdint>

class VulkanBuffer;

class InstanceBufferView {
public:
    InstanceBufferView() = default;
    InstanceBufferView(VulkanBuffer& buffer, uint32_t instance_count, uint32_t instance_size);

    bool valid() const;
    uint32_t instance_count() const;
    uint32_t instance_size() const;
    VulkanBuffer* buffer();

private:
    VulkanBuffer* m_buffer = nullptr;
    uint32_t m_instance_count = 0;
    uint32_t m_instance_size = 0;
};