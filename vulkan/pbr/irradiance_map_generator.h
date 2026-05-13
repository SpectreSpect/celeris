#pragma once
// #include <vulkan/vulkan.h>
// #include <glm/glm.hpp>

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
#include "../../path_utils.h"
// #include "../../math_utils.h"
// #include <utility>

class Mesh;

class IrradianceMapGenerator {
public:
    struct IrradianceMapGeneratorUniform {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t num_layers;
    };

    IrradianceMapGenerator() = default;
    IrradianceMapGenerator(VulkanEngine& engine);
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
    VulkanShaderModule equirect_to_cubemap_cs;
    
    uint32_t compute_queue_family_id;
    VkQueue compute_queue;
};