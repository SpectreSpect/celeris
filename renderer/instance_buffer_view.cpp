#include "instance_buffer_view.h"

#include "../vulkan_self/vulkan_buffer.h"

InstanceBufferView::InstanceBufferView(VulkanBuffer& buffer, uint32_t instance_count, uint32_t instance_size)
    :   m_buffer(&buffer),
        m_instance_count(instance_count),
        m_instance_size(instance_size) {}

bool InstanceBufferView::valid() const noexcept {
    if (!m_buffer)
        return false;
    
    if (m_buffer->size() < (m_instance_count * m_instance_size))
        return false;
    
    return true;
}

uint32_t InstanceBufferView::instance_count() const noexcept {
    return m_instance_count;
}

void InstanceBufferView::set_instance_count(uint32_t instance_count) {
    LOG_METHOD();

    logger.check(valid(), "Instance buffer is not valid. Maybe you're doing something wrong?");

    m_instance_count = instance_count;
}

uint32_t InstanceBufferView::instance_size() const noexcept {
    return m_instance_size;
}

const VulkanBuffer& InstanceBufferView::buffer() const {
    LOG_METHOD();

    logger.check(m_buffer != nullptr, "The instance buffer pointer specify to null");

    return *m_buffer;
}

VulkanBuffer& InstanceBufferView::buffer() {
    LOG_METHOD();

    logger.check(m_buffer != nullptr, "The instance buffer pointer specify to null");

    return *m_buffer;
}
