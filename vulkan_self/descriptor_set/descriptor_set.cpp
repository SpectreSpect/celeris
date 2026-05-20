#include "descriptor_set.h"
#include "../vulkan_buffer.h"
#include "../vulkan_command_buffer.h"
#include "../pipeline/vulkan_pipeline_layout.h"
#include "../pipeline/pipeline.h"


void DescriptorSet::write_buffer(uint32_t binding, VulkanBuffer& buffer, VkDescriptorType descriptor_type) {
    LOG_METHOD();

    logger.check(buffer.size() > 0, "Video buffer size must be greater than 0");

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer.handle();
    buffer_info.offset = 0;
    buffer_info.range = buffer.size();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = descriptor_type;
    write.descriptorCount = 1;
    write.pBufferInfo = &buffer_info;

    vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void DescriptorSet::write_uniform_buffer(uint32_t binding, VulkanBuffer& buffer) {
    LOG_METHOD();
    write_buffer(binding, buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void DescriptorSet::write_storage_buffer(uint32_t binding, VulkanBuffer& buffer) {
    LOG_METHOD();
    write_buffer(binding, buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point) {
    LOG_METHOD();

    vkCmdBindDescriptorSets(command_buffer.handle(),
                            bind_point, 
                            pipeline_layout, 0, 1,
                            &m_descriptor_set, 0, nullptr);
}

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, VulkanPipelineLayout& pipeline_layout, VkPipelineBindPoint bind_point) {
    LOG_METHOD();
    
    bind(command_buffer, pipeline_layout.handle(), bind_point);
}

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, Pipeline& pipeline) {
    LOG_METHOD();

    bind(command_buffer, pipeline.layout(), pipeline.get_bind_point());
}