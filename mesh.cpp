#include "mesh.h"
#include "vulkan/renderer.h"

Mesh::Mesh(
    VulkanEngine* engine,
    std::vector<std::byte> vertex_data,
    std::vector<uint32_t> index_data,
    VideoBuffer& staging_buffer)
{
    staging_buffer.ensure_capacity(std::max(vertex_data.size(), index_data.size() * sizeof(uint32_t)));

    vertex_buffer = VideoBuffer(
        engine,
        engine->physicalDevice,
        vertex_data.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    index_buffer =  VideoBuffer(
        engine,
        engine->physicalDevice,
        index_data.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    staging_buffer.update_data(vertex_data.data(), vertex_data.size());

    VideoBuffer::copy_buffer();

    index_buffer.update_data(index_data, index_data_size_bytes);

    num_indices = index_data_size_bytes / sizeof(unsigned int);
}


void Mesh::draw(RenderState state) {
    state.renderer->render_mesh(*this, state);
}
