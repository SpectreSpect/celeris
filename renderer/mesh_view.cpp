#include "mesh_view.h"

MeshView::MeshView(VulkanBufferView vertex_buffer_view, VulkanBufferView index_buffer_view, uint32_t index_count) {
    m_vertex_buffer_view = vertex_buffer_view;
    m_index_buffer_view = index_buffer_view;
    m_index_count = index_count;
}

uint32_t MeshView::index_count() const noexcept {
    return m_index_count;
}

void MeshView::bind_vertex_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding, VkDeviceSize offset) {
    LOG_METHOD();

    logger.check(m_vertex_buffer_view.valid(), "Vertex buffer view was invalid");

    m_vertex_buffer_view.handle().bind_as_vertex_buffer(command_buffer, buffer_binding, offset);
}

void MeshView::bind_index_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding, VkDeviceSize offset) {
    LOG_METHOD();

    logger.check(m_index_buffer_view.valid(), "Index buffer view was invalid");

    m_index_buffer_view.handle().bind_as_index_buffer(command_buffer, offset);
}

bool MeshView::valid() const noexcept {
    return m_vertex_buffer_view.valid() && m_index_buffer_view.valid() && m_index_count > 0;
}