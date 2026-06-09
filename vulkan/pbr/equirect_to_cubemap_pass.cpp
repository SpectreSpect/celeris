#include "equirect_to_cubemap_pass.h"

#include <array>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "../../mesh.h"
#include "../vulkan_utils.h"

// #include "../../math_utils.h"
#include "../../path_utils.h"


EquirectToCubemapPass::EquirectToCubemapPass(VulkanEngine& engine) {
    create(engine);
}

void EquirectToCubemapPass::create(VulkanEngine& engine) {
    this->engine = &engine;
    
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    equirect_to_cubemap_cs.create(engine.device, executable_dir() / "shaders" / "equirect_to_cubemap.comp.spv");
    equirect_to_cubemap_uniform_buffer.create(engine, sizeof(EquirectToCubemapUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, equirect_to_cubemap_uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, equirect_to_cubemap_cs);

    fence = Fence(engine.device);
}

Cubemap EquirectToCubemapPass::generate(Texture2D& equirectangular_map, uint32_t face_size) {
    if (face_size == 0)
        throw std::runtime_error("face size must be greater than 0");

    if (!engine)
        throw std::runtime_error("engine was null");

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t mip_levels = Cubemap::calc_mip_levels(face_size);

    Cubemap cubemap;
    cubemap.create(*engine, face_size, mip_levels, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    EquirectToCubemapUniform uniform_data{};
    uniform_data.image_width = face_size;
    uniform_data.image_height = face_size;
    uniform_data.num_layers = 6;
    equirect_to_cubemap_uniform_buffer.update_data(&uniform_data, sizeof(EquirectToCubemapUniform));

    descriptor_set_bundle.bind_combined_image_sampler(1, equirectangular_map);
    descriptor_set_bundle.bind_image_storage(2, cubemap);

    uint32_t x_groups = vulkan_utils::div_up_u32(uniform_data.image_width, 256);
    uint32_t y_groups = uniform_data.image_height;
    uint32_t z_groups = uniform_data.num_layers;

    command_buffer.begin();

    // Write mip 0 with compute
    cubemap.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, z_groups);

    // Prepare for blit-based mip generation
    cubemap.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );

    // Generate ordinary mip chain for the environment map
    cubemap.generate_mipmaps(command_buffer);

    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    return cubemap;
}