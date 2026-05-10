#include "point_cloud_generator.h"


void PointCloudGenerator::create(VulkanEngine& engine) {
    this->engine = &engine;

    compute_queue_family_id = vulkan_utils::find_compute_queue_family(engine.physicalDevice);
    vkGetDeviceQueue(engine.device, compute_queue_family_id, 0, &compute_queue);

    command_pool.create(engine.device, engine.physicalDevice, compute_queue_family_id, compute_queue);
    command_buffer.create(command_pool);

    compute_shader.create(engine.device, "shaders/point_cloud/generation/generate_point_cloud.comp.spv");
    uniform_buffer.create(engine, sizeof(PointCloudGeneratorUniform));

    DescriptorSetBundleBuilder builder = DescriptorSetBundleBuilder();
    builder.add_uniform_buffer(0, uniform_buffer, VK_SHADER_STAGE_COMPUTE_BIT);
    // builder.add_combined_image_sampler(1, VK_SHADER_STAGE_COMPUTE_BIT);
    builder.add_storage_buffer(1, VK_SHADER_STAGE_COMPUTE_BIT); // output point instances
    builder.add_storage_buffer(2, VK_SHADER_STAGE_COMPUTE_BIT); // output normal buffer
    descriptor_set_bundle = builder.create(engine.device);

    pipeline.create(engine.device, descriptor_set_bundle, compute_shader);

    fence = Fence(engine.device);
}

PointCloud PointCloudGenerator::generate(glm::vec3 position, glm::vec3 rotation, float time, 
                                         glm::vec3 prev_position, glm::vec3 prev_rotation, float prev_time) {
    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // Cubemap cubemap;
    // cubemap.create(*engine, face_size, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    PointCloudGeneratorUniform uniform_data{};
    uniform_data.width = 3600;
    uniform_data.height = 16;
    uniform_data.max_range = 50;
    uniform_data.position = glm::vec4(position, 1.0f);
    uniform_data.rotation = glm::vec4(rotation, 1.0f);
    // uniform_data.image_width = face_size;
    // uniform_data.image_height = face_size;
    // uniform_data.num_layers = 6;
    uniform_buffer.update_data(&uniform_data, sizeof(PointCloudGeneratorUniform));

    // descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);
    

    // uint32_t width = 3600;
    // uint32_t height = 16;

    uint32_t num_points = uniform_data.width * uniform_data.height;

    PointCloud output_point_cloud;
    output_point_cloud.create(*engine, num_points);
    
    // VideoBuffer point_instance_buffer;
    // point_instance_buffer.create(*engine, num_points * sizeof(PointInstance));

    uint32_t x_groups = vulkan_utils::div_up_u32(uniform_data.width, 16);
    uint32_t y_groups = vulkan_utils::div_up_u32(uniform_data.height, 16);

    descriptor_set_bundle.bind_storage_buffer(1, output_point_cloud.get_instance_buffer());
    
    command_buffer.begin();

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     VK_ACCESS_SHADER_WRITE_BIT
    // );

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, 1);

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT,
    //     VK_ACCESS_SHADER_READ_BIT
    // );

    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);



    // point_cloud.set_points(stdpoint_instance_buffer, num_points);

    return output_point_cloud;
}

void PointCloudGenerator::generate(PointCloud& point_cloud, VideoBuffer& normal_buffer, glm::vec3 position, glm::vec3 rotation) {

    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // Cubemap cubemap;
    // cubemap.create(*engine, face_size, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    PointCloudGeneratorUniform uniform_data{};
    uniform_data.width = 3600;
    uniform_data.height = 16;
    uniform_data.max_range = 50;
    uniform_data.position = glm::vec4(position, 1.0f);
    uniform_data.rotation = glm::vec4(rotation, 1.0f);
    // uniform_data.image_width = face_size;
    // uniform_data.image_height = face_size;
    // uniform_data.num_layers = 6;
    uniform_buffer.update_data(&uniform_data, sizeof(PointCloudGeneratorUniform));

    // descriptor_set_bundle.bind_combined_image_sampler(1, environment_map);
    

    // uint32_t width = 3600;
    // uint32_t height = 16;

    uint32_t num_points = uniform_data.width * uniform_data.height;

    // PointCloud output_point_cloud;
    // output_point_cloud.create(*engine, num_points);
    
    // VideoBuffer point_instance_buffer;
    // point_instance_buffer.create(*engine, num_points * sizeof(PointInstance));

    uint32_t x_groups = vulkan_utils::div_up_u32(uniform_data.width, 16);
    uint32_t y_groups = vulkan_utils::div_up_u32(uniform_data.height, 16);

    descriptor_set_bundle.bind_storage_buffer(1, point_cloud.get_instance_buffer());
    descriptor_set_bundle.bind_storage_buffer(2, normal_buffer);
    
    command_buffer.begin();

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     VK_ACCESS_SHADER_WRITE_BIT
    // );

    command_buffer.bind_pipeline(pipeline);
    command_buffer.dispatch(x_groups, y_groups, 1);

    // cubemap.image_resource.transition_layout(
    //     command_buffer,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT,
    //     VK_ACCESS_SHADER_READ_BIT
    // );

    // command_buffer.memory_barrier(temp_storage_buffer);
    command_buffer.end();

    command_buffer.submit_and_wait(compute_queue, fence);
}


void PointCloudGenerator::generate_with_motion(PointCloudFrame* point_cloud_frames, uint32_t num_point_cloud_frames) {
    float pi = glm::pi<float>();

    const uint32_t forward_frames = 0;
    const uint32_t turn_frames    = 200;
    const uint32_t return_frames  = num_point_cloud_frames - forward_frames - turn_frames;

    const float start_x     = 0.0f;
    const float end_x       = 0.0f;   // "end of road"
    const float scanner_y   = 0.3f;

    for (uint32_t i = 0; i < num_point_cloud_frames; i++) {
        glm::vec3 position(0.0f);
        glm::vec3 rotation(0.0f);

        if (i < forward_frames) {
            float t = float(i) / float(forward_frames - 1);

            // Move from start_x -> end_x
            float x = glm::mix(start_x, end_x, t);
            position = glm::vec3(x, scanner_y, 0.0f);

            // Facing forward
            rotation = glm::vec3(0.0f, 0.0f, 0.0f);
        }
        else if (i < forward_frames + turn_frames) {
            float t = float(i - forward_frames) / float(turn_frames - 1);

            // Stay at the end of the road
            position = glm::vec3(end_x, scanner_y, 0.0f);

            // Turn from 0 -> 180 degrees
            float yaw = glm::mix(0.0f, pi, t);
            rotation = glm::vec3(0.0f, yaw, 0.0f);
        }
        else {
            float t = float(i - forward_frames - turn_frames) / float(return_frames - 1);

            // Move from end_x -> start_x
            float x = glm::mix(end_x, start_x, t);
            position = glm::vec3(x, scanner_y, 0.0f);

            // Facing backward
            rotation = glm::vec3(0.0f, pi, 0.0f);
        }

        uint32_t num_points = 3600 * 16;

        point_cloud_frames[i].point_cloud.create(*engine, num_points);
        point_cloud_frames[i].normal_buffer.create(*engine, num_points * sizeof(glm::vec4));

        generate(point_cloud_frames[i].point_cloud, point_cloud_frames[i].normal_buffer, position, rotation);
    }
}