#pragma once

#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/vulkan_command_buffer.h"

#include "../../vulkan_self/logger/logger.h"

class VulkanEngine;
class ComputePassManager;
class Cubemap;
class CubemapArray;

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
    void generate_into(Cubemap& environment_map, CubemapArray& irradiance_maps, uint32_t cubemap_id);

private:
    VulkanEngine& m_engine;

    VulkanBuffer uniform_buffer;

    PassInstance m_irradiance_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};
