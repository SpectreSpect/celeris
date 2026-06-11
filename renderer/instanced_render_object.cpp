#include "instanced_render_object.h"
#include "../vulkan_self/vulkan_engine.h"
#include "renderer.h"
#include "instance_batch.h"


InstancedRenderObject::InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material)
    :   RenderObject(mesh, material) {}

InstancedRenderObject::InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch& instance_batch)
    :   RenderObject(mesh, material),
        m_instance_buffer_view(instance_batch.get_view()) {}

InstancedRenderObject::InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material,
        VulkanBuffer& instance_buffer, uint32_t instance_count, uint32_t instance_size)
    :   RenderObject(mesh, material),
        m_instance_buffer_view(instance_buffer, instance_count, instance_size) {}

// InstancedRenderObject::InstancedRenderObject(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material,
//                                              uint32_t instance_count, uint32_t instance_size_bytes) 
//     :   RenderObject(mesh, material),
//         instance_data(engine, instance_count, instance_size_bytes){
// }

void InstancedRenderObject::render(Renderer& renderer, VulkanCommandBuffer& command_buffer, const glm::mat4& world_transform) {
    sync_material();
    renderer.render(command_buffer, *this, world_transform);
}

void InstancedRenderObject::set_instance_view(InstanceBufferView instance_buffer_view) {
    m_instance_buffer_view = instance_buffer_view;
}

bool InstancedRenderObject::instance_buffer_view_valid() const {
    return m_instance_buffer_view.valid();
}

uint32_t InstancedRenderObject::instance_count() const {
    return m_instance_buffer_view.instance_count();
}

void InstancedRenderObject::set_instance_count(uint32_t count) {
    LOG_METHOD();

    logger.check(instance_buffer_view_valid(), "Instance buffer must be valid");
    
    m_instance_buffer_view.set_instance_count(count);
}

uint32_t InstancedRenderObject::instance_size() const {
    return m_instance_buffer_view.instance_size();
}

InstanceBufferView InstancedRenderObject::instance_view() const {
    return m_instance_buffer_view;
}

const VulkanBuffer& InstancedRenderObject::instance_buffer() const {
    LOG_METHOD();

    return m_instance_buffer_view.buffer();
}

VulkanBuffer& InstancedRenderObject::instance_buffer() {
    LOG_METHOD();

    return m_instance_buffer_view.buffer();
}
