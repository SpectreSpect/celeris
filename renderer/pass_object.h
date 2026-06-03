#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/pipeline/pipeline_pass.h"
#include "../vulkan_self/pipeline/vulkan_pipeline_layout.h"

class VulkanBuffer;
class VulkanTexture2D;
class VulkanCommandBuffer;

class PassObject {
public:
    _XPARENT_NAME(PassObject);

    PassObject(PipelinePass& pass);
    virtual ~PassObject() noexcept = default;

    PassObject(const PassObject&) = delete;
    PassObject& operator=(const PassObject&) = delete;

    PassObject(PassObject&&) noexcept = default;
    PassObject& operator=(PassObject&&) noexcept = default;

    virtual void set_uniform_buffer(uint32_t binding, VulkanBuffer& uniform_buffer) = 0;
    virtual void set_storage_buffer(uint32_t binding, VulkanBuffer& storage_buffer) = 0;
    virtual void set_texture(uint32_t binding, VulkanTexture2D& texture_2d) = 0;

    template <class... Args>
    void push_constants(VulkanCommandBuffer& command_buffer, Args&&... args) const {
        LOG_METHOD();

        logger.check(m_pipeline_pass != nullptr, "Compute pass pointer specify to null");

        m_pipeline_pass->pipeline_layout().push_constants(command_buffer, std::forward<Args>(args)...);
    }

    // Descriptor set or descriptor writer
    virtual void bind_description_object(VulkanCommandBuffer& command_buffer) = 0;

    void bind(VulkanCommandBuffer& command_buffer);

    PipelinePass& pipepline_pass();  
    const PipelinePass& pipepline_pass() const;  

private:
    PipelinePass* m_pipeline_pass = nullptr;
};
