#pragma once

#include "transform.h"
#include "mesh.h"

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/material/material_instance_temp.h"

class MaterialInstance;

class RenderObject {
public:
    _XCLASS_NAME(RenderObject);

    Transform transform;

    // RenderObject(Mesh& mesh, MaterialInstanceTemp& material);
    RenderObject(Mesh& mesh, MaterialInstance& material);

    Mesh& m_mesh;
    // MaterialInstanceTemp* m_material = nullptr;
    MaterialInstance* m_material = nullptr;
    uint32_t material_data_id;
};