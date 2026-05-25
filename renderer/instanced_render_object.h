#pragma once

#include "render_object.h"
#include "instance_batch.h"
#include "instance_buffer_view.h"

class VulkanEngine;

class InstancedRenderObject : public RenderObject {
public:
    _XCHILD_NAME(InstancedRenderObject);

    InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material);
    InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, InstanceBatch& instance_batch);
    InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, MaterialInstance& material, 
        VulkanBuffer& instance_buffer, uint32_t instance_count, uint32_t instance_size);
    virtual void render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);

    void set_instance_view(InstanceBufferView instance_buffer_view);
    bool instance_buffer_view_valid() const;
    uint32_t instance_count() const;
    uint32_t instance_size() const;
    InstanceBufferView get_instance_view() const;
    VulkanBuffer* instance_buffer();

private:
    // InstanceBatch instance_data;
    InstanceBufferView m_instance_buffer_view;
};