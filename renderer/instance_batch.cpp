#include "instance_batch.h"

#include "../vulkan_self/vulkan_engine.h"

InstanceBatch::InstanceBatch(VulkanPhysicalDevice& physical_device, VulkanDevice& device,  uint32_t instance_count, 
                             uint32_t instance_size_bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties) 
    :   m_buffer(physical_device, device, instance_count * instance_size_bytes, usage, memory_properties),
        m_instance_count(instance_count) {}

InstanceBatch::InstanceBatch(VulkanPhysicalDevice& physical_device, VulkanDevice& device,  uint32_t instance_count, uint32_t instance_size_bytes) 
    :   m_buffer(VulkanBuffer(physical_device, device, instance_count * instance_size_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)),
        m_instance_count(instance_count) {}

InstanceBatch::InstanceBatch(VulkanEngine& engine, uint32_t instance_count, uint32_t instance_size_bytes) 
    :   m_buffer(VulkanBuffer(engine.physical_device(), engine.device(), instance_count * instance_size_bytes, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)),
        m_instance_count(instance_count) {}
    
// InstanceBatch::InstanceBatch(VulkanEngine& engine, VulkanBuffer& m_buffer, uint32_t instance_count, uint32_t instance_size_bytes)
//     :   m_buffer(m_buffer),
//         m_instance_count(instance_count) {}

VulkanBuffer& InstanceBatch::buffer() {
    return m_buffer;
}

uint32_t InstanceBatch::instance_count() const {
    return m_instance_count;
}

void InstanceBatch::set_instance_count(uint32_t instance_count) {
    logger.check(instance_count <= m_instance_count, "Tried to increase instance_count. TODO note: this shouldn't be like this. Ideally, you shoulnd't be able to change the instance count like that without changing the size of the video buffer. This should be changed.");
    m_instance_count = instance_count;
}