#include "render_object.h"
#include "renderer.h"

RenderObject::RenderObject(Mesh& mesh, SlotPassInstance& material)
    :   m_mesh_view(mesh.get_view()),
        m_material(&material),
        m_material_data_id(material.slot_buffer().allocate_slot()) {}

RenderObject::RenderObject(MeshView mesh_view, SlotPassInstance& material)
    : m_mesh_view(mesh_view),
      m_material(&material),
      m_material_data_id(material.slot_buffer().allocate_slot()) {}

RenderObject::~RenderObject() {
    if (m_material && m_material_data_id != UINT32_MAX) {
        m_material->slot_buffer().free_slot(m_material_data_id);
    }
}

RenderObject::RenderObject(RenderObject&& other) noexcept
    : SceneObject(std::move(other)),
      m_mesh_view(other.m_mesh_view),
      m_material(other.m_material),
      m_material_data_id(other.m_material_data_id)
{
    other.m_material = nullptr;
    other.m_material_data_id = UINT32_MAX;
}

RenderObject& RenderObject::operator=(RenderObject&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (m_material && m_material_data_id != UINT32_MAX) {
        m_material->slot_buffer().free_slot(m_material_data_id);
    }

    transform = std::move(other.transform);
    m_material = other.m_material;
    m_material_data_id = other.m_material_data_id;

    other.m_material = nullptr;
    other.m_material_data_id = UINT32_MAX;

    return *this;
}

void RenderObject::sync_material() {
    m_material->sync();
}

MeshView& RenderObject::mesh_view() noexcept {
    return m_mesh_view;
}

SlotPassInstance& RenderObject::material() noexcept {
    return *m_material;
}

uint32_t RenderObject::material_data_id() const noexcept {
    return m_material_data_id;
}

void RenderObject::render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform) {
    sync_material();
    renderer.render(command_buffer, *this, world_transform);
}
