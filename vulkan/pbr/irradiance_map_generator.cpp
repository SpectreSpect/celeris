#include "irradiance_map_generator.h"

#include <array>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>

#include "../../mesh.h"
#include "../vulkan_utils.h"

// #include "../../math_utils.h"


IrradianceMapGenerator::IrradianceMapGenerator(VulkanEngine& engine) {
    create(engine);
}

void IrradianceMapGenerator::create(VulkanEngine& engine) {
    this->engine = &engine;

    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    equirect_to_cubemap_cs.create(engine.device, executable_dir() / "shaders" / "generate_irradiance_map.comp.spv");
    uniform_buffer.create(engine, sizeof(IrradianceMapGeneratorUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_image_storage(2, VK_SHADER_STAGE_COMPUTE_BIT);
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, equirect_to_cubemap_cs);

    fence = Fence(engine.device);
}

Cubemap IrradianceMapGenerator::generate(Cubemap& environment_map, uint32_t face_size) {
    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT |
    //                           VK_IMAGE_USAGE_STORAGE_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_DST_BIT |
    //                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
    //                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        

    Cubemap cubemap;
    cubemap.create(*engine, face_size, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    IrradianceMapGeneratorUniform uniform_data{};
    uniform_data.image_width = face_size;
    uniform_data.image_height = face_size;
    uniform_data.num_layers = 6;
    uniform_buffer.update_data(&uniform_data, sizeof(IrradianceMapGeneratorUniform));

    descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);
    descriptor_set_bundle.bind_image_storage(2, cubemap);

    uint32_t x_groups = vulkan_utils::div_up_u32(face_size, 16);
    uint32_t y_groups = vulkan_utils::div_up_u32(face_size, 16);
    uint32_t z_groups = uniform_data.num_layers;
    
    command_buffer.begin();

    cubemap.image_resource.transition_layout(
        command_buffer,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );

    cubemap.image_resource.

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, z_groups);

    cubemap.image_resource.transition_layout(
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

    return cubemap;
}