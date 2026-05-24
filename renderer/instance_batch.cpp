#include "instance_batch.h"

InstanceBatch::InstanceBatch(VulkanPhysicalDevice& physical_device, VulkanDevice& device,  uint32_t instance_count, 
                             uint32_t instance_size_bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties) 
    :   m_buffer(physical_device, device, instance_count * instance_size_bytes, usage, memory_properties),
        m_instance_count(instance_count) {}

InstanceBatch::InstanceBatch(VulkanPhysicalDevice& physical_device, VulkanDevice& device,  uint32_t instance_count, uint32_t instance_size_bytes) 
    :   m_buffer(VulkanBuffer::create_host_visible_vertex_buffer(physical_device, device, instance_count * instance_size_bytes)),
        m_instance_count(instance_count) {}

InstanceBatch::InstanceBatch(VulkanEngine& engine, uint32_t instance_count, uint32_t instance_size_bytes) 
    :   m_buffer(VulkanBuffer::create_host_visible_vertex_buffer(engine, instance_count * instance_size_bytes)),
        m_instance_count(instance_count) {}
    
VulkanBuffer& InstanceBatch::buffer() {
    return m_buffer;
}

uint32_t InstanceBatch::instance_count() const {
    return m_instance_count;
}