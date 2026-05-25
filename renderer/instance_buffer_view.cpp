#include "instance_buffer_view.h"

#include "../vulkan_self/vulkan_buffer.h"

InstanceBufferView::InstanceBufferView(VulkanBuffer& buffer, uint32_t instance_count, uint32_t instance_size)
    :   m_buffer(&buffer),
        m_instance_count(instance_count),
        m_instance_size(instance_size) {}

bool InstanceBufferView::valid() const {
    if (!m_buffer)
        return false;
    
    if (m_buffer->size() < (m_instance_count * m_instance_size))
        return false;
    
    return true;
}

uint32_t InstanceBufferView::instance_count() const {
    return m_instance_count;
}

uint32_t InstanceBufferView::instance_size() const {
    return m_instance_size;
}

VulkanBuffer* InstanceBufferView::buffer() {
    return m_buffer;
}