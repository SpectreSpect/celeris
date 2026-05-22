#include "render_object.h"

RenderObject::RenderObject(Mesh& mesh, MaterialInstanceTemp& material) : m_mesh(mesh) {
    m_material = &material;
}