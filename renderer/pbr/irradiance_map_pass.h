#pragma once

#include "../compute_pass_instance.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/vulkan_command_buffer.h"

#include "../../../vulkan_self/logger/logger.h"

class VulkanEngine;
class ComputePassManager;
class Cubemap;

class IrradiancePass {
public:
    _XCLASS_NAME(IrradiancePass);

    struct IrradianceMapGeneratorUniform {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t num_layers;
    };

    IrradiancePass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    Cubemap generate(Cubemap& environment_map, uint32_t face_size);

private:
    VulkanEngine& m_engine;

    VulkanBuffer uniform_buffer;

    ComputePassInstance m_irradiance_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};
