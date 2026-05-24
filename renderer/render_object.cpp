#include "render_object.h"
#include "../vulkan_self/material/material_instance.h"
#include "renderer.h"

RenderObject::~RenderObject() {
    if (m_material && material_data_id != UINT32_MAX) {
        m_material->material_buffer.free_slot(material_data_id);
    }
}

RenderObject::RenderObject(RenderObject&& other) noexcept
    : SceneObject(std::move(other)),
      m_mesh(other.m_mesh),
      m_material(other.m_material),
      material_data_id(other.material_data_id)
{
    other.m_material = nullptr;
    other.material_data_id = UINT32_MAX;
}

RenderObject& RenderObject::operator=(RenderObject&& other) noexcept {
    if (this == &other) {
        return *this;
    }

    if (m_material && material_data_id != UINT32_MAX) {
        m_material->material_buffer.free_slot(material_data_id);
    }

    transform = std::move(other.transform);
    m_material = other.m_material;
    material_data_id = other.material_data_id;

    other.m_material = nullptr;
    other.material_data_id = UINT32_MAX;

    return *this;
}

void RenderObject::sync_material() {
    m_material->material_buffer.sync();
}

void RenderObject::render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform) {
    sync_material();
    renderer.render(command_buffer, *this, world_transform);
}