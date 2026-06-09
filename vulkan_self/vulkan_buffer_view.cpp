#include "vulkan_buffer_view.h"
#include "vulkan_buffer.h"

VulkanBufferView::VulkanBufferView(VulkanBuffer& vukan_buffer) : m_buffer(&vukan_buffer) {};

bool VulkanBufferView::valid() const noexcept {
    return m_buffer;
}

VulkanBuffer& VulkanBufferView::handle() const noexcept {
    return *m_buffer;
}