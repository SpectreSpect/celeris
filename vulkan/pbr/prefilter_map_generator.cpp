#include "prefilter_map_generator.h"

#include <array>
#include <stdexcept>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>

#include "../../mesh.h"
#include "../vulkan_utils.h"

PrefilterMapGenerator::PrefilterMapGenerator(VulkanEngine& engine) {
    create(engine);
}

void PrefilterMapGenerator::create(VulkanEngine& engine) {
    this->engine = &engine;

    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    generate_prefilter_map_cs.create(engine.device, executable_dir() / "shaders" / "generate_prefilter_map.comp.spv");
    uniform_buffer.create(engine, sizeof(PrefilterMapGeneratorUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, generate_prefilter_map_cs);

    fence = Fence(engine.device);
}

Cubemap PrefilterMapGenerator::generate(Cubemap& environment_map, uint32_t face_size) {
    if (!engine)
        throw std::runtime_error("engine was null");

    if (face_size == 0)
        throw std::runtime_error("face size must be greater than 0");

    uint32_t mip_levels = Cubemap::calc_mip_levels(face_size);

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT;

    Cubemap prefilter_map;
    prefilter_map.create(*engine, face_size, mip_levels, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);

    // Transition whole output image to GENERAL once
    command_buffer.begin();
    prefilter_map.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );
    command_buffer.end();
    CommandBuffer::SubmitDesc submit_desc{};
    submit_desc.queue = compute_queue;
    submit_desc.fence = &fence;

    command_buffer.submit(submit_desc);
    fence.wait_for_fence();

    for (uint32_t mip = 0; mip < mip_levels; ++mip) {
        uint32_t mip_face_size = std::max(1u, face_size >> mip);
        float roughness = (mip_levels > 1)
            ? static_cast<float>(mip) / static_cast<float>(mip_levels - 1)
            : 0.0f;

        PrefilterMapGeneratorUniform uniform_data{};
        uniform_data.face_size = mip_face_size;
        uniform_data.num_layers = 6;
        uniform_data.roughness = roughness;
        uniform_data.environment_resolution = static_cast<float>(environment_map.face_size);

        uniform_buffer.update_data(&uniform_data, sizeof(PrefilterMapGeneratorUniform));

        VulkanImageView mip_view;
        mip_view.create(
            *engine,
            VulkanImageView::cubemap_mip_desc(&prefilter_map.image_resource, mip)
        );

        descriptor_set_bundle.bind_image_storage(2, mip_view);

        uint32_t x_groups = vulkan_utils::div_up_u32(mip_face_size, 16);
        uint32_t y_groups = vulkan_utils::div_up_u32(mip_face_size, 16);
        uint32_t z_groups = 6;

        command_buffer.begin();
        command_buffer.bind_pipeline(pipeline);
        command_buffer.dispatch(x_groups, y_groups, z_groups);
        command_buffer.end();

        command_buffer.submit_and_wait(compute_queue, fence);

        mip_view.destroy(engine->device);
    }

    command_buffer.begin();
    prefilter_map.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    return prefilter_map;
}