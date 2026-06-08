#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "../../logger/logger_header.h"
#include "pass_object.h"
#include "../../descriptor_set/descriptor_set.h"

class DescriptorPool;
class VulkanCommandBuffer;
class VulkanBuffer;
class VulkanTexture2D;
class PipelinePass;
class Cubemap;
class VulkanImageView;

class PassInstance : virtual public PassObject {
public:
    _XCHILD_NAME(PassInstance);

    PassInstance(PipelinePass& pass, DescriptorPool& pool, uint32_t instance_set_id = 0);

    virtual void set_uniform_buffer(uint32_t binding, const VulkanBuffer& uniform_buffer) override;
    virtual void set_storage_buffer(uint32_t binding, const VulkanBuffer& storage_buffer) override;
    virtual void set_texture(uint32_t binding, const VulkanTexture2D& texture_2d) override;
    void set_storage_texture(uint32_t binding, const VulkanTexture2D& texture_2d);
    void set_cubemap(uint32_t binding, const Cubemap& cubemap);
    void set_storage_cubemap(uint32_t binding, const Cubemap& cubemap);
    void set_storage_image_view(uint32_t binding, const VulkanImageView& image_view);

    virtual void bind_description_object(VulkanCommandBuffer& command_buffer) override;

    DescriptorSet& descripter_set() noexcept;
    const DescriptorSet& descripter_set() const noexcept;

private:
    DescriptorSet m_descriptor_set;
    uint32_t m_instance_set_id = 0;
};
