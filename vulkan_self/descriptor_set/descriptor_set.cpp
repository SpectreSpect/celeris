#include "descriptor_set.h"
#include "../vulkan_buffer.h"
#include "../vulkan_command_buffer.h"
#include "../vulkan_pipeline_layout.h"


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

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, VulkanPipelineLayout& pipeline_layout) {
    LOG_METHOD();
    vkCmdBindDescriptorSets(command_buffer.handle(), 
                            VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            pipeline_layout.handle(), 0, 1,
                            &m_descriptor_set, 0, nullptr);
}