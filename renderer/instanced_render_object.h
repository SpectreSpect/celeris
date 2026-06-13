#pragma once

#include "render_object.h"
#include "instance_batch.h"
#include "instance_buffer_view.h"

class VulkanEngine;

class InstancedRenderObject : public RenderObject {
public:
    _XCHILD_NAME(InstancedRenderObject);

    InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material);
    InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch& instance_batch);
    InstancedRenderObject(
        VulkanEngine& engine,
        Mesh& mesh,
        SlotPassInstance& material,
        VulkanBuffer& instance_buffer,
        uint32_t instance_count,
        uint32_t instance_size
    );

    uint32_t instance_count() const;
    void set_instance_count(uint32_t count);

    uint32_t instance_size() const;

    const VulkanBuffer& instance_buffer() const;
    VulkanBuffer& instance_buffer();

    InstanceBufferView instance_view() const;
    void set_instance_view(InstanceBufferView instance_buffer_view);

    bool instance_buffer_view_valid() const;
    virtual void render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);

private:
    // InstanceBatch instance_data;
    InstanceBufferView m_instance_buffer_view;
};
