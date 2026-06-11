#include "render_object.h"

#include <utility>

#include "renderer.h"

RenderObject::RenderObject(Mesh& mesh, SlotPassInstance& material)
    :   m_mesh_view(mesh.get_view()),
        m_material(&material),
        m_material_data_id(material.slot_buffer().allocate_slot()) {}

RenderObject::RenderObject(MeshView mesh_view, SlotPassInstance& material)
    : m_mesh_view(mesh_view),
      m_material(&material),
      m_material_data_id(material.slot_buffer().allocate_slot()) {}

RenderObject::~RenderObject() noexcept {
    destroy();
}

void RenderObject::destroy() noexcept {
    if (m_material && m_material_data_id != UINT32_MAX) {
        m_material->slot_buffer().free_slot(m_material_data_id);
    }

    m_material = nullptr;
    m_material_data_id = UINT32_MAX;
}

RenderObject::RenderObject(RenderObject&& other) noexcept
    : SceneObject(std::move(other)),
      m_mesh_view(std::move(other.m_mesh_view)),
      m_material(std::exchange(other.m_material, nullptr)),
      m_material_data_id(std::exchange(other.m_material_data_id, UINT32_MAX)) {}

RenderObject& RenderObject::operator=(RenderObject&& other) noexcept {
    if (this != &other) {
        destroy();

        SceneObject::operator=(std::move(other));

        m_mesh_view = std::move(other.m_mesh_view);
        m_material = std::exchange(other.m_material, nullptr);
        m_material_data_id = std::exchange(other.m_material_data_id, UINT32_MAX);
    }

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
