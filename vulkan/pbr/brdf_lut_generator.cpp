#include "brdf_lut_generator.h"

#include <array>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "../../mesh.h"
#include "../vulkan_utils.h"

// #include "../../math_utils.h"


BrdfLutGenerater::BrdfLutGenerater(VulkanEngine& engine) {
    create(engine);
}

void BrdfLutGenerater::create(VulkanEngine& engine) {
    this->engine = &engine;
    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    generate_brdf_lut_cs.create(engine.device, executable_dir() / "shaders"/ "generate_brdf_lut.comp.spv");
    uniform_buffer.create(engine, sizeof(BrdfLutGeneratorUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(1, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, generate_brdf_lut_cs);

    fence = Fence(engine.device);
}

Texture2D BrdfLutGenerater::generate(uint32_t width, uint32_t height) {
    if (width <= 0 || height <= 0)
        throw std::runtime_error("both width and hegiht must be greater than 0");

    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
    //                           VK_IMAGE_USAGE_STORAGE_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    //                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        

    Texture2D brdf_lut_texture;
    brdf_lut_texture.create(*engine, width, height, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    BrdfLutGeneratorUniform uniform_data{};
    uniform_data.image_width = width;
    uniform_data.image_height = height;
    uniform_buffer.update_data(&uniform_data, sizeof(BrdfLutGeneratorUniform));

    descriptor_set_bundle.bind_image_storage(1, brdf_lut_texture);

    uint32_t x_groups = vulkan_utils::div_up_u32(width, 16);
    uint32_t y_groups = vulkan_utils::div_up_u32(height, 16);
    
    command_buffer.begin();

    brdf_lut_texture.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, 1);

    brdf_lut_texture.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT
    );

    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);

    fence.wait_for_fence();

    return brdf_lut_texture;
}