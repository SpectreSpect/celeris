#include "indirect_render_object.h"
#include "renderer.h"

IndirectRenderObject::IndirectRenderObject(Mesh& mesh, SlotPassInstance& material, VulkanBuffer& indirect_buffer, uint32_t max_draw_count)
    :   RenderObject(mesh, material), 
        m_max_draw_count(max_draw_count),
        m_indirect_buffer_view(indirect_buffer) {}

IndirectRenderObject::IndirectRenderObject(MeshView& mesh_view, SlotPassInstance& material, VulkanBuffer& indirect_buffer, uint32_t max_draw_count)
    :   RenderObject(mesh_view, material), 
        m_max_draw_count(max_draw_count),
        m_indirect_buffer_view(indirect_buffer) {}

void IndirectRenderObject::render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform) {
    sync_material();
    renderer.render(command_buffer, *this, world_transform);
}

void IndirectRenderObject::set_indirect_buffer_view(VulkanBufferView indirect_buffer_view) {
    m_indirect_buffer_view = indirect_buffer_view;
}

bool IndirectRenderObject::indirect_buffer_view_valid() const {
    return m_indirect_buffer_view.valid() && m_max_draw_count > 0;
}

uint32_t IndirectRenderObject::max_draw_count() const noexcept {
    return m_max_draw_count;
}

VulkanBufferView IndirectRenderObject::get_indirect_buffer_view() const {
    return m_indirect_buffer_view;
}

VulkanBuffer* IndirectRenderObject::indirect_buffer() {
    return &m_indirect_buffer_view.handle();
}