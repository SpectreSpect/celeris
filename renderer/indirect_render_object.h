#pragma once

#include "render_object.h"
#include "instance_batch.h"
#include "instance_buffer_view.h"

class VulkanEngine;

class IndirectRenderObject : public RenderObject {
public:
    _XCHILD_NAME(InstancedRenderObject);

    IndirectRenderObject(Mesh& mesh, SlotPassInstance& material, VulkanBuffer& indirect_buffer, uint32_t max_draw_count);
    
    virtual void render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform);

    void set_indirect_buffer_view(VulkanBufferView indirect_buffer_view);
    bool indirect_buffer_view_valid() const;
    uint32_t max_draw_count() const noexcept;
    VulkanBufferView get_indirect_buffer_view() const;
    VulkanBuffer* indirect_buffer();

private:
    uint32_t m_max_draw_count = 0;
    VulkanBufferView m_indirect_buffer_view;
};
