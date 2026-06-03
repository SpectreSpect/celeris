#include "descriptor_set.h"
#include "../vulkan_buffer.h"
#include "../vulkan_command_buffer.h"
#include "../pipeline/vulkan_pipeline_layout.h"
#include "../pipeline/pipeline.h"
#include "../image/vulkan_texture_2d.h"
#include "../image/cubemap.h"
#include "../image/cubemap_array.h"
#include "../image/vulkan_image_view.h"

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

void DescriptorSet::write_texture(
    uint32_t binding,
    const VulkanTexture2D& texture)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_descriptor_set != VK_NULL_HANDLE, "Descriptor set is not initialized");

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

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        m_device,
        1,
        &write,
        0,
        nullptr
    );
}

void DescriptorSet::write_cubemap(
    uint32_t binding,
    const Cubemap& cubemap)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_descriptor_set != VK_NULL_HANDLE, "Descriptor set is not initialized");

    logger.check(cubemap.view().handle() != VK_NULL_HANDLE, "Cubemap image view is not initialized");
    logger.check(cubemap.sampler().handle() != VK_NULL_HANDLE, "Cubemap sampler is not initialized");

    // Use this check if your Cubemap class tracks its current layout.
    logger.check(
        cubemap.layout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        "Cubemap image layout must be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
    );

    VkDescriptorImageInfo image_info{};
    image_info.sampler = cubemap.sampler().handle();
    image_info.imageView = cubemap.view().handle();
    image_info.imageLayout = cubemap.layout();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        m_device,
        1,
        &write,
        0,
        nullptr
    );
}

void DescriptorSet::write_cubemap_array(
    uint32_t binding,
    const CubemapArray& cubemap_array)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_descriptor_set != VK_NULL_HANDLE, "Descriptor set is not initialized");

    logger.check(cubemap_array.view().handle() != VK_NULL_HANDLE, "Cubemap array image view is not initialized");
    logger.check(cubemap_array.sampler().handle() != VK_NULL_HANDLE, "Cubemap array sampler is not initialized");
    logger.check(
        cubemap_array.layout() == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        "Cubemap array image layout must be VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL"
    );

    VkDescriptorImageInfo image_info{};
    image_info.sampler = cubemap_array.sampler().handle();
    image_info.imageView = cubemap_array.view().handle();
    image_info.imageLayout = cubemap_array.layout();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        m_device,
        1,
        &write,
        0,
        nullptr
    );
}

void DescriptorSet::write_storage_cubemap(
    uint32_t binding,
    const Cubemap& cubemap)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_descriptor_set != VK_NULL_HANDLE, "Descriptor set is not initialized");

    logger.check(cubemap.view().handle() != VK_NULL_HANDLE, "Cubemap image view is not initialized");

    // logger.check(
    //     cubemap.layout() == VK_IMAGE_LAYOUT_GENERAL,
    //     "Cubemap image layout must be VK_IMAGE_LAYOUT_GENERAL for storage image"
    // );

    VkDescriptorImageInfo image_info{};
    image_info.sampler = VK_NULL_HANDLE;
    image_info.imageView = cubemap.view().handle();
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        m_device,
        1,
        &write,
        0,
        nullptr
    );
}

void DescriptorSet::write_storage_texture(uint32_t binding, const VulkanTexture2D& texture) {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_descriptor_set != VK_NULL_HANDLE, "Descriptor set is not initialized");

    logger.check(texture.view().handle() != VK_NULL_HANDLE, "Texture image view is not initialized");

    // Use this if your VulkanTexture2D tracks its current layout.
    // For storage images, the image must be in VK_IMAGE_LAYOUT_GENERAL.
    logger.check(
        texture.texture_layout() == VK_IMAGE_LAYOUT_GENERAL,
        "Texture image layout must be VK_IMAGE_LAYOUT_GENERAL for storage image"
    );

    VkDescriptorImageInfo image_info{};
    image_info.sampler = VK_NULL_HANDLE;
    image_info.imageView = texture.view().handle();
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        m_device,
        1,
        &write,
        0,
        nullptr
    );
}

void DescriptorSet::write_storage_image_view(
    uint32_t binding,
    const VulkanImageView& image_view)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    logger.check(m_descriptor_set != VK_NULL_HANDLE, "Descriptor set is not initialized");

    logger.check(
        image_view.handle() != VK_NULL_HANDLE,
        "Storage image view is not initialized"
    );

    VkDescriptorImageInfo image_info{};
    image_info.sampler = VK_NULL_HANDLE;
    image_info.imageView = image_view.handle();
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = m_descriptor_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(
        m_device,
        1,
        &write,
        0,
        nullptr
    );
}

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, VkPipelineLayout pipeline_layout, VkPipelineBindPoint bind_point, uint32_t set_binding) {
    LOG_METHOD();

    vkCmdBindDescriptorSets(command_buffer.handle(),
                            bind_point, 
                            pipeline_layout, set_binding, 1,
                            &m_descriptor_set, 0, nullptr);
}

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, VulkanPipelineLayout& pipeline_layout, VkPipelineBindPoint bind_point, uint32_t set_binding) {
    LOG_METHOD();
    
    bind(command_buffer, pipeline_layout.handle(), bind_point, set_binding);
}

void DescriptorSet::bind(VulkanCommandBuffer& command_buffer, Pipeline& pipeline, uint32_t set_binding) {
    LOG_METHOD();

    bind(command_buffer, pipeline.layout(), pipeline.get_bind_point(), set_binding);
}
