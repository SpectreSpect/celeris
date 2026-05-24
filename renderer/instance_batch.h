#pragma once

#include "../vulkan_self/logger/logger_header.h"

#include "../vulkan_self/vulkan_buffer.h"

class InstanceBatch {
public:
    _XCLASS_NAME(InstanceBatch);

    InstanceBatch(VulkanPhysicalDevice& physical_device, VulkanDevice& device,  
                  uint32_t instance_count, uint32_t instance_size_bytes,
                  VkBufferUsageFlags usage, VkMemoryPropertyFlags memory_properties);
    
    InstanceBatch(VulkanPhysicalDevice& physical_device, VulkanDevice& device,  
                  uint32_t instance_count, uint32_t instance_size_bytes);
    
    InstanceBatch(VulkanEngine& engine, uint32_t instance_count, uint32_t instance_size_bytes);

    InstanceBatch(VulkanEngine& engine, VulkanBuffer& m_buffer, uint32_t instance_count, uint32_t instance_size_bytes);

    

    VulkanBuffer& buffer();
    uint32_t instance_count() const;
    void set_instance_count(uint32_t instance_count);

    VulkanBuffer* external_buffer = nullptr;

private:
    VulkanBuffer m_buffer;
    uint32_t m_instance_count;
};