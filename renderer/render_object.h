#pragma once

#include "transform.h"
#include "mesh.h"

#include "../vulkan_self/logger/logger_header.h"

class RenderObject {
public:
    _XCLASS_NAME(RenderObject);

    Transform transform;

    RenderObject(Mesh& mesh);

    Mesh& m_mesh;
};