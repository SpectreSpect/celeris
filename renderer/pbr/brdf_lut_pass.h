#pragma once

#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/vulkan_command_buffer.h"

#include "../../vulkan_self/logger/logger.h"

class VulkanEngine;
class VulkanTexture2D;
class ComputePassManager;
class Cubemap;

class BrdfLutPass {
public:
    _XCLASS_NAME(BrdfLutPass);

    struct BrdfLutGeneratorUniform {
        uint32_t image_width;
        uint32_t image_height;
    };

    BrdfLutPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    VulkanTexture2D generate(uint32_t width, uint32_t height);

private:
    void generate_cubemap_mipmaps(
        VulkanCommandBuffer& command_buffer,
        Cubemap& cubemap
    );

    VulkanEngine& m_engine;

    VulkanBuffer uniform_buffer;

    PassInstance m_brdf_lut_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};
