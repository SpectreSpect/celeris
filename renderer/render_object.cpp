#include "render_object.h"
#include "../vulkan_self/material/material_instance.h"

// RenderObject::RenderObject(Mesh& mesh, MaterialInstanceTemp& material) : m_mesh(mesh) {
//     m_material = &material;
// }

RenderObject::RenderObject(Mesh& mesh, MaterialInstance& material) : m_mesh(mesh) {
    m_material = &material;
    material_data_id = material.material_buffer.allocate_slot();
}