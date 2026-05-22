#pragma once

#include "render_object.h"

class InstancedRenderObject : public RenderObject {
public:
    _XCHILD_NAME(InstancedRenderObject);

    InstancedRenderObject(Mesh& mesh, MaterialInstanceTemp& material);

    InstanceBatch instance_data;
};