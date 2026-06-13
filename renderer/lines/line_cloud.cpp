#include "line_cloud.h"
#include "../instance_batch.h"
#include "../../vulkan_self/vulkan_engine.h"
#include "../mesh.h"
#include "line_instance.h"

LineCloud::LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch&& instance_batch) 
    :   InstancedRenderObject(engine, mesh, material),
        m_instance_batch(std::make_unique<InstanceBatch>(std::move(instance_batch)))
{
    LOG_METHOD();
    set_instance_view(m_instance_batch->get_view());
}

LineCloud::LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, InstanceBatch& instance_batch)
    :   InstancedRenderObject(engine, mesh, material, instance_batch),
        m_instance_batch(nullptr) {}

LineCloud::LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, uint32_t instance_count)
    :   InstancedRenderObject(engine, mesh, material),
        m_instance_batch(std::make_unique<InstanceBatch>(
            engine, instance_count, sizeof(LineInstance)
        ))
{
    LOG_METHOD();
    set_instance_view(m_instance_batch->get_view());
}

LineCloud::LineCloud(VulkanEngine& engine, Mesh& mesh, SlotPassInstance& material, const std::vector<LineInstance>& lines)
    :   LineCloud(engine, mesh, material, lines.size())
{
    LOG_METHOD();
    set_lines(lines);
}

void LineCloud::set_lines(const std::vector<LineInstance>& lines) {
    LOG_METHOD();

    logger.check(!lines.empty(), "Lines vector was empty");

    set_instance_count(lines.size());
    instance_buffer().upload(lines);
}
