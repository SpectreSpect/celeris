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


glm::vec3 PointCloudGenerator::safe_normalize(glm::vec3 v, glm::vec3 fallback) {
    float len2 = glm::dot(v, v);
    if (len2 < 1e-12f) {
        return fallback;
    }
    return v / std::sqrt(len2);
}

glm::vec3 PointCloudGenerator::euler_xyz_from_mat3(const glm::mat3& R) {
    // GLM is column-major: R[col][row]
    //
    // For R = Rz * Ry * Rx:
    //
    // r20 = -sin(y)
    // r21 = cos(y) * sin(x)
    // r22 = cos(y) * cos(x)
    // r10 = sin(z) * cos(y)
    // r00 = cos(z) * cos(y)

    float r00 = R[0][0];
    float r10 = R[0][1];
    float r20 = R[0][2];

    float r21 = R[1][2];
    float r22 = R[2][2];

    float y = std::asin(std::clamp(-r20, -1.0f, 1.0f));
    float cy = std::cos(y);

    float x;
    float z;

    if (std::abs(cy) > 1e-6f) {
        x = std::atan2(r21, r22);
        z = std::atan2(r10, r00);
    } else {
        // Gimbal lock fallback.
        // We choose x = 0 and solve z from the remaining matrix terms.
        x = 0.0f;
        z = std::atan2(-R[1][0], R[1][1]);
    }

    return glm::vec3{x, y, z}; // pitch, yaw, roll in radians
}

glm::vec3 PointCloudGenerator::lidar_rotation_from_camera_front_up(glm::vec3 camera_front, glm::vec3 camera_up_hint) {
    // Your LiDAR shader convention:
    //
    // local +X = forward
    // local +Y = up
    // local +Z = right

    glm::vec3 front = safe_normalize(camera_front, glm::vec3(1.0f, 0.0f, 0.0f));
    glm::vec3 upHint = safe_normalize(camera_up_hint, glm::vec3(0.0f, 1.0f, 0.0f));

    glm::vec3 right = glm::cross(front, upHint);

    if (glm::dot(right, right) < 1e-12f) {
        // front and up were almost parallel
        glm::vec3 fallbackUp = std::abs(front.y) < 0.99f
            ? glm::vec3(0.0f, 1.0f, 0.0f)
            : glm::vec3(1.0f, 0.0f, 0.0f);

        right = glm::cross(front, fallbackUp);
    }

    right = glm::normalize(right);
    glm::vec3 up = glm::normalize(glm::cross(right, front));

    glm::mat3 R;

    // Columns are local axes expressed in world space.
    R[0] = front; // local +X
    R[1] = up;    // local +Y
    R[2] = right; // local +Z

    return euler_xyz_from_mat3(R);
}

glm::vec3 PointCloudGenerator::lidar_rotation_from_camera(const Camera& camera) {
    return lidar_rotation_from_camera_front_up(camera.front, camera.up);
}

void PointCloudGenerator::write_camera_transform_header(std::ofstream& out) {
    out << "# frame px py pz rx ry rz fx fy fz ux uy uz\n";
}

void PointCloudGenerator::write_camera_transform(std::ofstream& out, uint32_t frame_index, const Camera& camera) {
    glm::vec3 rotation = lidar_rotation_from_camera(camera);

    out << frame_index << ' '

        << camera.position.x << ' '
        << camera.position.y << ' '
        << camera.position.z << ' '

        << rotation.x << ' '
        << rotation.y << ' '
        << rotation.z << ' '

        << camera.front.x << ' '
        << camera.front.y << ' '
        << camera.front.z << ' '

        << camera.up.x << ' '
        << camera.up.y << ' '
        << camera.up.z << '\n';
}

void PointCloudGenerator::save_camera_transformations(const std::string& path, const std::vector<Camera>& cameras) {
    std::ofstream out(path);

    if (!out) {
        throw std::runtime_error("Failed to open camera transformation file for writing: " + path);
    }

    out << std::fixed << std::setprecision(9);

    write_camera_transform_header(out);

    for (uint32_t i = 0; i < cameras.size(); ++i) {
        write_camera_transform(out, i, cameras[i]);
    }
}

std::vector<PointCloudGenerator::LidarTransformFrame> PointCloudGenerator::load_lidar_transformations(const std::string& path) {
    std::ifstream in(path);

    if (!in) {
        throw std::runtime_error("Failed to open camera transformation file for reading: " + path);
    }

    std::vector<LidarTransformFrame> frames;

    std::string line;
    uint32_t line_number = 0;

    while (std::getline(in, line)) {
        ++line_number;

        if (line.empty()) {
            continue;
        }

        if (line[0] == '#') {
            continue;
        }

        std::istringstream iss(line);

        LidarTransformFrame frame{};

        if (!(iss
            >> frame.frame_index

            >> frame.position.x
            >> frame.position.y
            >> frame.position.z

            >> frame.rotation.x
            >> frame.rotation.y
            >> frame.rotation.z
        )) {
            throw std::runtime_error(
                "Failed to parse camera transformation file at line " +
                std::to_string(line_number)
            );
        }

        frames.push_back(frame);
    }

    return frames;
}

uint32_t PointCloudGenerator::generate_from_camera_transform_file(
    PointCloudFrame* point_cloud_frames,
    uint32_t max_point_cloud_frames,
    int width,
    int height,
    const std::string& path
) {
    std::vector<LidarTransformFrame> transforms = load_lidar_transformations(path);

    uint32_t frames_to_generate = std::min<uint32_t>(
        max_point_cloud_frames,
        static_cast<uint32_t>(transforms.size())
    );

    uint32_t num_points = static_cast<uint32_t>(width * height);

    for (uint32_t i = 0; i < frames_to_generate; ++i) {
        const LidarTransformFrame& transform = transforms[i];

        glm::vec3 position = transform.position;
        glm::vec3 rotation = transform.rotation;

        point_cloud_frames[i].point_cloud.create(*engine, num_points);
        point_cloud_frames[i].normal_buffer.create(*engine, num_points * sizeof(glm::vec4));

        generate(
            point_cloud_frames[i].point_cloud,
            point_cloud_frames[i].normal_buffer,
            position,
            rotation,
            width,
            height
        );



        point_cloud_frames[i].points.resize(num_points);
        point_cloud_frames[i].point_cloud.get_instance_buffer().read_subdata(
            0,
            point_cloud_frames[i].points.data(),
            num_points * sizeof(PointInstance)
        );

        point_cloud_frames[i].normals.resize(num_points);
        point_cloud_frames[i].normal_buffer.read_subdata(
            0,
            point_cloud_frames[i].normals.data(),
            num_points * sizeof(glm::vec4)
        );

        point_cloud_frames[i].point_cloud.color = glm::vec4(1, 0, 0, 1);

        // point_cloud_frames[i].point_cloud.position = position;
        // point_cloud_frames[i].point_cloud.rotation = rotation;
    }

    return frames_to_generate;
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

void PointCloudGenerator::generate(PointCloud& point_cloud, VideoBuffer& normal_buffer, glm::vec3 position, glm::vec3 rotation, int width, int height) {

    if (!this->engine)
        throw std::runtime_error("engine was null");
    
    // VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    // Cubemap cubemap;
    // cubemap.create(*engine, face_size, 1, usage, VK_FORMAT_R32G32B32A32_SFLOAT);

    PointCloudGeneratorUniform uniform_data{};
    uniform_data.width = width;
    uniform_data.height = height;
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


void PointCloudGenerator::generate_with_motion(PointCloudFrame* point_cloud_frames, uint32_t num_point_cloud_frames, int width, int height) {
    float pi = glm::pi<float>();

    const uint32_t forward_frames = 5;
    const uint32_t turn_frames    = 20;
    const uint32_t return_frames  = num_point_cloud_frames - forward_frames - turn_frames;

    const float start_x     = 0.0f;
    const float end_x       = 5.0f;   // "end of road"
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

        uint32_t num_points = width * height;

        point_cloud_frames[i].point_cloud.create(*engine, num_points);
        point_cloud_frames[i].normal_buffer.create(*engine, num_points * sizeof(glm::vec4));

        // if (i == 80)
        //     std::cout << 'sdf' << std::endl;
        
        // if (i == 0)
        //     std::cout << 'sdf' << std::endl;

        generate(point_cloud_frames[i].point_cloud, point_cloud_frames[i].normal_buffer, position, rotation, width, height);

        point_cloud_frames[i].points.resize(num_points);
        point_cloud_frames[i].point_cloud.get_instance_buffer().read_subdata(0, point_cloud_frames[i].points.data(), num_points * sizeof(PointInstance));

        point_cloud_frames[i].normals.resize(num_points);
        point_cloud_frames[i].normal_buffer.read_subdata(0, point_cloud_frames[i].normals.data(), num_points * sizeof(glm::vec4));

        point_cloud_frames[i].point_cloud.color = glm::vec4(1, 0, 0, 1);

        // point_cloud_frames[i].point_cloud.instance_buffer
        // point_cloud_frames[i].points = 
    }
}