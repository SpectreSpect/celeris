#pragma once

#include <vulkan/vulkan.h>

#include "../logger/logger_header.h"

class VulkanDevice;
class VulkanShaderModule;
class VulkanPipelineLayout;
class VulkanCommandBuffer;

class Pipeline {
public:
    Pipeline() = default;

    Pipeline(Pipeline&) = delete;
    Pipeline& operator=(Pipeline&) = delete;

    Pipeline(Pipeline&& other) noexcept;
    Pipeline& operator=(Pipeline&& other) noexcept;

    ~Pipeline();
    
private:
    void destroy() noexcept;

public:

    virtual VkPipelineBindPoint get_bind_point() const noexcept = 0;
    void bind(VulkanCommandBuffer& command_buffer) const;

    VkPipeline handle() const noexcept;
    VkDevice device() const noexcept;
    VkPipelineLayout layout() const noexcept;

protected:
    void set_pipeline(VkDevice device, VkPipeline pipeline, VkPipelineLayout layout);

private:
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};