#include "descripter_set_writer.h"

#include "../vulkan_buffer.h"
#include "../image/vulkan_texture_2d.h"
#include "../pipeline/pipeline.h"
#include "../vulkan_command_buffer.h"

DescriptorSetWriter& DescriptorSetWriter::write_buffer(uint32_t binding, const VulkanBuffer& buffer, VkDescriptorType descriptor_type) {
    LOG_METHOD();

    logger.check(buffer.handle() != VK_NULL_HANDLE, "Buffer is not initialized");
    logger.check(buffer.size() > 0, "Video buffer size must be greater than 0");

    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = buffer.handle();
    buffer_info.offset = 0;
    buffer_info.range = buffer.size();

    m_buffer_infos.push_back(buffer_info);

    PendingWrite write{
        .binding = binding,
        .descriptor_type = descriptor_type,
        .info_index = m_buffer_infos.size() - 1,
        .is_buffer = true
    };

    m_pending_writes.push_back(write);

    return *this;
}

DescriptorSetWriter& DescriptorSetWriter::write_uniform_buffer(uint32_t binding, const VulkanBuffer& buffer) {
    LOG_METHOD();
    return write_buffer(binding, buffer, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

DescriptorSetWriter& DescriptorSetWriter::write_storage_buffer(uint32_t binding, const VulkanBuffer& buffer) {
    LOG_METHOD();
    return write_buffer(binding, buffer, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

DescriptorSetWriter& DescriptorSetWriter::write_texture(uint32_t binding, const VulkanTexture2D& texture) {
    LOG_METHOD();

    logger.check(texture.view().handle() != VK_NULL_HANDLE, "Texture image view is not initialized");
    logger.check(texture.sampler().handle() != VK_NULL_HANDLE, "Texture sampler is not initialized");
    logger.check(
        texture.texture_layout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        "Texture image layout must be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
    );

    VkDescriptorImageInfo image_info{};
    image_info.sampler = texture.sampler().handle();
    image_info.imageView = texture.view().handle();
    image_info.imageLayout = texture.texture_layout();

    m_image_infos.push_back(image_info);

    PendingWrite write{
        .binding = binding,
        .descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .info_index = m_image_infos.size() - 1,
        .is_buffer = false
    };

    m_pending_writes.push_back(write);

    return *this;
}

void DescriptorSetWriter::clear_writes() {
    LOG_METHOD();

    m_buffer_infos.clear();
    m_image_infos.clear();
    m_pending_writes.clear();
}

void DescriptorSetWriter::push_descriptor_set(VulkanCommandBuffer& command_buffer, const Pipeline& pipeline, uint32_t set_index) const {
    LOG_METHOD();

    logger.check(command_buffer.handle() != VK_NULL_HANDLE, "Command buffer is not initialized");
    logger.check(pipeline.layout() != VK_NULL_HANDLE, "Pipeline layout is not initialized");
    logger.check(m_pending_writes.size() > 0, "Attempt to push descripter set with 0 writes");

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(m_pending_writes.size());

    for (const PendingWrite& pending : m_pending_writes) {
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = pending.binding;
        write.dstArrayElement = 0;
        write.descriptorType = pending.descriptor_type;
        write.descriptorCount = 1;

        if (pending.is_buffer) {
            write.pBufferInfo = &m_buffer_infos[pending.info_index];
        } else {
            write.pImageInfo = &m_image_infos[pending.info_index];
        }

        writes.push_back(write);
    }

    vkCmdPushDescriptorSetKHR(
        command_buffer.handle(),
        pipeline.get_bind_point(),
        pipeline.layout(),
        set_index,
        static_cast<uint32_t>(writes.size()),
        writes.data()
    );
}

void DescriptorSetWriter::push_descriptor_set_and_clear(VulkanCommandBuffer& command_buffer, const Pipeline& pipeline, uint32_t set_index) {
    LOG_METHOD();
    push_descriptor_set(command_buffer, pipeline, set_index);
    clear_writes();
}
