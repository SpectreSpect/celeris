#pragma once

#include <cstdint>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/descriptor_set/descripter_set_writer.h"
#include "pass_object.h"

class VulkanDevice;
class PipelinePass;

class PassWriter : public PassObject {
public:
    _XCHILD_NAME(PassWriter);

    PassWriter(VulkanDevice& device, PipelinePass& pass);
    virtual ~PassWriter() noexcept override = default;

    PassWriter(const PassWriter&) = delete;
    PassWriter& operator=(const PassWriter&) = delete;

    PassWriter(PassWriter&&) noexcept = default;
    PassWriter& operator=(PassWriter&&) noexcept = default;

    virtual void set_uniform_buffer(uint32_t binding, VulkanBuffer& uniform_buffer) override;
    virtual void set_storage_buffer(uint32_t binding, VulkanBuffer& storage_buffer) override;
    virtual void set_texture(uint32_t binding, VulkanTexture2D& texture_2d) override;

    virtual void bind_description_object(VulkanCommandBuffer& command_buffer) override;

private:
    DescriptorSetWriter m_writer;
};
