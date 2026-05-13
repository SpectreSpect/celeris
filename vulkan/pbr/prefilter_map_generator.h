#pragma once

#include "../../vulkan_engine.h"
#include "../compute_pipeline.h"
#include "../fence.h"
#include "../image/texture2d.h"
#include "../image/cubemap.h"
#include "../graphics_pipeline.h"
#include "../descriptor_set_bundle.h"
#include "../render_target_2d.h"
#include "../command_pool.h"
#include "../command_buffer.h"
#include "../image/image_view.h"
#include "../../path_utils.h"

class Mesh;

class PrefilterMapGenerator {
public:
    struct PrefilterMapGeneratorUniform {
        uint32_t face_size;
        uint32_t num_layers;
        float roughness;
        float environment_resolution;
    };

    PrefilterMapGenerator() = default;
    PrefilterMapGenerator(VulkanEngine& engine);

    void create(VulkanEngine& engine);
    Cubemap generate(Cubemap& environment_map, uint32_t face_size);
    void destroy();

    CommandPool command_pool;
    CommandBuffer command_buffer;

private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    VulkanShaderModule generate_prefilter_map_cs;
    uint32_t compute_queue_family_id = 0;
    VkQueue compute_queue = VK_NULL_HANDLE;
};