#pragma once

#include "render_object.h"
#include "instance_batch.h"

class VulkanEngine;

class InstancedRenderObject : public RenderObject {
public:
    _XCHILD_NAME(InstancedRenderObject);

    InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, uint32_t instance_count, uint32_t instance_size_bytes);
    virtual void render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);

    InstanceBatch instance_data;
};