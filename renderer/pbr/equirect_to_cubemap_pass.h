#pragma once

#include "../compute_pass_instance.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/vulkan_command_buffer.h"

#include "../../../vulkan_self/logger/logger.h"

class VulkanTexture2D;
class ComputePassManager;
class Cubemap;

class EquirectToCubemapPass {
public:
    _XCLASS_NAME(EquirectToCubemapPass);

    struct EquirectToCubemapUniform {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t num_layers;
    };

    EquirectToCubemapPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    Cubemap generate(VulkanTexture2D& equirectangular_map, uint32_t face_size);

private:
    void generate_cubemap_mipmaps(
        VulkanCommandBuffer& command_buffer,
        Cubemap& cubemap
    );

    VulkanEngine& m_engine;

    VulkanBuffer uniform_buffer;

    ComputePassInstance m_equirect_to_cubemap_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};