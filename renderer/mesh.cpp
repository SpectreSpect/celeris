#include "mesh.h"

Mesh::Mesh(VulkanEngine& engine, VulkanResourceLoader& resource_loader, 
           void* vertex_data, uint32_t vertex_data_size_bytes, 
           unsigned int* index_data, uint32_t index_data_size_bytes) 
    :   m_vertex_buffer(VulkanBuffer::create_vertex_buffer(engine, vertex_data_size_bytes)),
        m_index_buffer(VulkanBuffer::create_index_buffer(engine, index_data_size_bytes))
{
    LOG_METHOD();

    logger.check(vertex_data != nullptr, "Failed to create mesh: vertex data is null");
    logger.check(index_data != nullptr, "Failed to create mesh: index data is null");

    logger.check(vertex_data_size_bytes > 0, "Failed to create mesh: vertex data size is 0");
    logger.check(index_data_size_bytes > 0, "Failed to create mesh: index data size is 0");

    m_index_count = index_data_size_bytes / sizeof(unsigned int);

    logger.check(
        m_index_count >= 3,
        "Failed to create mesh: index count must be at least 3"
    );

    logger.check(
        m_index_count % 3 == 0,
        "Failed to create mesh: index count must be divisible by 3 for triangle list topology"
    );

    resource_loader.upload_vertex_buffer(vertex_data, vertex_data_size_bytes, m_vertex_buffer);
    resource_loader.upload_index_buffer(index_data, index_data_size_bytes, m_index_buffer);
    resource_loader.submit();
}

uint32_t Mesh::index_count() const noexcept {
    return m_index_count;
}

void Mesh::bind_vertex_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding, VkDeviceSize offset) {
    m_vertex_buffer.bind_as_vertex_buffer(command_buffer, buffer_binding, offset);
}

void Mesh::bind_index_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding, VkDeviceSize offset) {
    m_index_buffer.bind_as_index_buffer(command_buffer, offset);
}