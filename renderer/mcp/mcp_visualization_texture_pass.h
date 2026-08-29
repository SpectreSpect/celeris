#pragma once

#include <cstdint>

#include "../../vulkan_self/logger/logger.h"
#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"

class ComputePassManager;
class VulkanEngine;
class VulkanTexture2D;

class McpVisualizationTexturePass {
public:
    _XCLASS_NAME(McpVisualizationTexturePass);

    McpVisualizationTexturePass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    VulkanTexture2D generate(uint32_t width, uint32_t height);
    void render(VulkanTexture2D& texture);

private:
    VulkanEngine& m_engine;
    PassInstance m_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};
