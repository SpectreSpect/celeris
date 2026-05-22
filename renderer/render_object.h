#pragma once

#include "transform.h"
#include "mesh.h"

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/material/material_instance_temp.h"

class RenderObject {
public:
    _XCLASS_NAME(RenderObject);

    Transform transform;

    RenderObject(Mesh& mesh, MaterialInstanceTemp& material);

    Mesh& m_mesh;
    MaterialInstanceTemp* m_material = nullptr;
};