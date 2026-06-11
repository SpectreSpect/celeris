#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/vulkan_shader_module.h"
#include "vulkan_self/vulkan_buffer.h"
#include "vulkan_self/vulkan_resource_loader.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout_builder.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout.h"
#include "vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "vulkan_self/descriptor_set/descriptor_pool.h"
#include "vulkan_self/descriptor_set/descriptor_set.h"
#include "vulkan_self/pipeline/vulkan_pipeline_layout.h"
#include "vulkan_self/pipeline/graphics_pipeline/graphics_pipeline.h"
#include "vulkan_self/pipeline/compute_pipeline/compute_pipeline.h"
#include "vulkan_self/pass/pipeline_pass_builder.h"
#include "vulkan_self/pass/compute_pass/compute_pass.h"
#include "vulkan_self/image/vulkan_image.h"
#include "vulkan_self/image/cpu_image.h"
#include "vulkan_self/image/vulkan_texture_2d.h"
#include "path_utils.h"
#include "renderer/resources/frame_resources.h"
#include "camera/camera.h"
#include "camera/controllers/fps_camera_controller.h"
#include "renderer/transform.h"
#include "renderer/mesh.h"
#include "renderer/render_object.h"
#include "renderer/transform_push_constants.h"
#include "managers/shader_manager.h"
#include "managers/material_manager.h"
#include "vulkan_self/pass/compute_pass/compute_pass_builder.h"
#include "managers/compute_pass_manager.h"
#include "renderer/instanced_render_object.h"
#include "renderer/instance_batch.h"
#include "managers/texture_manager.h"
#include "managers/material_instance_manager.h"
#include "renderer/point_cloud/point_instance.h"
#include "renderer/renderer.h"
#include "managers/mesh_manager.h"
#include "managers/manager_bundle.h"
#include "renderer/point_cloud/point_cloud.h"
#include "renderer/scene.h"
#include "renderer/skybox.h"
#include "renderer/point_cloud/lidar/lidar_scan.h"
#include "renderer/point_cloud/lidar/lidar_video.h"
#include "renderer/point_cloud/gicp/gicp_pass.h"
#include "renderer/point_cloud/gicp/voxel_point_map.h"
#include "renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "renderer/point_cloud/gicp/voxel_map_point_reseter.h"
// #include "renderer/point_cloud/lidar/lidar_scan_receiver.h"
#include "imgui_layer.h"
#include "renderer/lighting_system/lighting_system.h"
#include "renderer/pbr/equirect_to_cubemap_pass.h"
#include "renderer/pbr/brdf_lut_pass.h"
#include "renderer/pbr/prefilter_map_pass.h"
#include "renderer/pbr/irradiance_map_pass.h"
#include "vulkan_self/image/cubemap_array.h"
#include "voxel_grid_vulkan/voxel_grid.h"
#include "renderer/static_mesh_data.h"
#include "renderer/indirect_render_object.h"
#include "voxel_grid_vulkan/voxelizator.h"
#include "renderer/point_cloud/point_cloud_mesher.h"
#include "renderer/point_cloud/point_cloud_preprocessor.h"
#include "math_utils.h"
#include "renderer/point_cloud/point_instance.h"
#include "renderer/lines/line_cloud.h"
#include "renderer/lines/line_instance.h"

#include <vector>
#include <random>

// static std::mt19937 rng(std::random_device{}());
// static std::uniform_real_distribution<double> dist(0.0, 1.0);

VkClearValue clear_color = {0.05f, 0.05f, 0.05f, 1.0f};

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Celeris");

    QueueRequest queue_request;
    queue_request.graphics_count = 2;
    queue_request.present_count = 1;
    queue_request.compute_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);

    UI ui(window, engine);
    Camera camera;
    FPSCameraController camera_controller(camera);
    camera_controller.speed = 20;

    VulkanResourceLoader resource_loader(engine, 154217728); // 1 Мб

    ShaderManager shader_manager(engine.device());
    ComputePassManager compute_pass_manager(engine.device(), shader_manager);
    TextureManager texture_manager(engine, resource_loader, compute_pass_manager);
    
    LightingSystem lighting_system(engine, compute_pass_manager);
    FrameResources frame_resources(engine, lighting_system, engine.num_frames_in_flight());

    lighting_system.set_frame_resources(frame_resources);

    MaterialManager material_manager(engine, shader_manager, frame_resources);
    MaterialInstanceManager material_instance_manager(engine, material_manager, texture_manager);
    MeshManager mesh_manager(engine, resource_loader);
    ManagerBundle manager_bundle(engine, shader_manager, texture_manager, material_manager, material_instance_manager, mesh_manager);

    PointCloudPreprocessor point_cloud_preprocessor(
        engine.device(), 
        engine.compute_queue(),
        compute_pass_manager
    );

    // LidarScanReceiver scan_receiver(point_cloud_preprocessor, 5000);
    // scan_receiver.start();

    std::unique_ptr<LidarScan> network_scan;
    // std::deque<std::unique_ptr<LidarScan>> retired_network_scans;

    glm::vec3 voxel_size(1.0f);
    glm::ivec3 chunk_size(16);
    VoxelGrid::VoxelGridDesc voxel_grid_desc {
        .chunk_size = chunk_size,
        .voxel_size = voxel_size,
        .count_active_chunks = 10'000,
        .max_quads = 1'000'000,
        .chunk_hash_table_size_factor = 1.0f,
        .count_evict_buckets = 32,
        .min_free_chunks = 4'500,
        .tomb_fraction_to_rebuild = 0.2f,
        .eviction_bucket_shell_thickness = chunk_size.x * voxel_size.x * 1,
        .vb_page_size_order_of_two = 10,
        .ib_page_size_order_of_two = 10,
        .buddy_allocator_nodes_factor = 1.0,
        .render_distance = chunk_size.x * voxel_size.x * 30,
        .generation_distance = 10,
        .max_write_count = chunk_size.x * chunk_size.y * chunk_size.z * static_cast<uint32_t>(2'000)
    };

    VoxelGrid voxel_grid(
        engine.physical_device(),
        engine.device(),
        engine.compute_queue(),
        compute_pass_manager,
        material_instance_manager,
        voxel_grid_desc
    );

    Voxelizator::VoxelizatorDesc voxelizator_desc {
        .chunk_size = chunk_size,
        .voxel_size = voxel_size,
        .counter_hash_table_size = 1'000'000,
        .count_voxel_writes = 0 // Будут использоваться те, что внутри voxel_grid
    };

    Voxelizator voxelizator(
        engine.physical_device(),
        engine.device(),
        engine.compute_queue(),
        compute_pass_manager,
        voxelizator_desc
    );

    uint32_t count_points_in_lidar_ring = 3600;
    uint32_t count_rings_in_lidar = 16;
    uint32_t count_triangles_in_polygon_ribbon = count_points_in_lidar_ring * 2 - 2;
    uint32_t count_polygon_ribbons = count_rings_in_lidar - 1;
    uint32_t total_count_triangles_in_scan = count_polygon_ribbons * count_triangles_in_polygon_ribbon;

    VulkanBuffer scan_vertex_buffer(
        engine.physical_device(),
        engine.device(),
        sizeof(PBRVertex) * total_count_triangles_in_scan * 3,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VulkanBuffer scan_index_buffer(
        engine.physical_device(),
        engine.device(),
        sizeof(uint32_t) * total_count_triangles_in_scan * 3,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    
    VoxelWriteGPU blue_voxelize_prefab;
    blue_voxelize_prefab.voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, glm::ivec3({0, 98, 255}));
    blue_voxelize_prefab.set_flags = OVERWRITE_BIT;

    VoxelWriteGPU transparent_voxelize_prefab;
    transparent_voxelize_prefab.voxel_data = VoxelDataGPU(0, 0, glm::ivec3(255));
    transparent_voxelize_prefab.set_flags = OVERWRITE_BIT;

    PointCloudMesher mesher(
        engine.physical_device(),
        engine.device(),
        engine.compute_queue(),
        compute_pass_manager,
        3600
    );

    LidarScan lidar_scan(manager_bundle, point_cloud_preprocessor, path_utils::executable_dir() / "assets" / "lidar_scans" / "frame_000000.bin");
    uint32_t scan_index_count = mesher.convert_to_mesh(
        lidar_scan.point_cloud(),
        scan_vertex_buffer,
        scan_index_buffer,
        sizeof(PointInstance),
        offsetof(PointInstance, pos),
        sizeof(PBRVertex),
        offsetof(PBRVertex, position),
        offsetof(PBRVertex, normal)
    );

    MeshView scan_mesh_view(
        scan_vertex_buffer.get_view(),
        scan_index_buffer.get_view(),
        scan_index_count
    );

    RenderObject scan_object(scan_mesh_view, material_instance_manager.pbr);
    // RenderObject scan_object(mesh_manager.cube.get_view(), material_instance_manager.pbr);
    scan_object.set_material_data(PBRMaterialData::create(0.0f, 0.95f, 1.8f, glm::vec4(1.0f), 1.0f));

    scan_object.transform.scale = glm::vec3(1.0f);

    voxelizator.voxelize_and_submit(
        blue_voxelize_prefab,
        scan_object.mesh_view(),
        offsetof(PBRVertex, position),
        sizeof(PBRVertex),
        scan_object.transform.get_model_matrix(),
        &voxel_grid.local_voxel_write_list()
    );

    glm::ivec3 block_size = glm::ivec3(10, 20, 30);
    glm::ivec3 block_origin = glm::ivec3(0, 30, 0);
    std::vector<VoxelWriteGPU> test_voxel_writes;
    test_voxel_writes.reserve(static_cast<size_t>(block_size.x * block_size.y * block_size.z));

    for (int x = 0; x < block_size.x; x++)
        for (int y = 0; y < block_size.y; y++)
            for (int z = 0; z < block_size.z; z++) {
                glm::ivec3 base_color{0, 98, 255};
                glm::ivec3 color = glm::vec3(base_color) * glm::vec3(0.5 + math_utils::dist(math_utils::rng) * 0.5);

                test_voxel_writes.push_back(
                    VoxelWriteGPU{
                        .world_voxel = glm::ivec4(block_origin, 0) + glm::ivec4(x, y, z, 0),
                        .voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, color),
                        .set_flags = OVERWRITE_BIT
                    }
                );
            }

    VulkanBuffer box_voxel_write_list = VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t) * 4 + Utils::size_bytes(test_voxel_writes));
    box_voxel_write_list.upload_scalar<uint32_t>(test_voxel_writes.size(), 0);
    box_voxel_write_list.upload(test_voxel_writes, sizeof(uint32_t) * 4);

    VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    {
        auto scope = compute_command_buffer.begin_scope();
        voxel_grid.set_voxels(compute_command_buffer, box_voxel_write_list);
    }
    VulkanFence compute_fence(engine.device());
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();

    uint32_t max_write_count = 100000;
    VulkanBuffer voxel_write_list = VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * max_write_count);

    GICPPass gicp_pass(engine, compute_pass_manager);
    VoxelMapPointInserter voxel_map_inserter(engine, compute_pass_manager);
    VoxelMapPointReseter voxel_map_reseter(engine, compute_pass_manager);

    Renderer renderer(engine, frame_resources);
    
    RenderObject sphere(mesh_manager.sphere, material_instance_manager.pbr);

    RenderObject vox_box(mesh_manager.cube, material_instance_manager.pbr);
    vox_box.transform.position = glm::vec3(0.0f, 80.0f, 0.0f);
    vox_box.transform.scale = glm::vec3(20.0f);

    const float skybox_exposure = 1.8f;

    Skybox skybox(
        mesh_manager.skybox_cube,
        material_instance_manager.st_peters_square_night_4k_hdr,
        texture_manager,
        material_manager.pbr_mp,
        TextureManager::st_peters_square_night_4k_pbr_map_id,
        skybox_exposure
    );

    
    constexpr uint32_t segment_count = 128;

    constexpr float x_start = -10.0f;
    constexpr float x_end = 10.0f;

    constexpr float amplitude = 2.0f;
    constexpr float frequency = 1.0f;

    const glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

    std::vector<LineInstance> lines;
    lines.reserve(segment_count);

    auto make_point = [](float x) {
        constexpr float amplitude = 2.0f;
        constexpr float frequency = 1.0f;

        return glm::vec3(
            x,
            0.0f,
            std::sin(x * frequency) * amplitude
        );
    };

    for (uint32_t i = 0; i < segment_count; i++) {
        float t0 = static_cast<float>(i) / static_cast<float>(segment_count);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(segment_count);

        float x0 = glm::mix(x_start, x_end, t0);
        float x1 = glm::mix(x_start, x_end, t1);

        lines.push_back(LineInstance{
            .p0 = make_point(x0),
            .p1 = make_point(x1),
            .color = color
        });
    }

    LineCloud line_cloud(
        engine,
        mesh_manager.line_quad,
        material_instance_manager.line,
        lines
    );

    line_cloud.set_material_data(LineMaterialData{
        .color = glm::vec4(245.0f, 176.0f, 66.0f, 255.0f) * glm::vec4(1/255.0f),
        .line_width_pixels = 10
    });
    line_cloud.sync_material();

    
    // LidarVideo lidar_video(manager_bundle, 
    //                        point_cloud_preprocessor,
    //                        "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 0, 1);

    // Important: save original first frame pose before overwriting it.
    // glm::vec3 first_position = lidar_video.get_scan(0).point_cloud().transform.position;
    // glm::quat first_rotation = glm::normalize(lidar_video.get_scan(0).point_cloud().transform.rotation);

    // for (int i = static_cast<int>(lidar_video.get_scan_count()) - 1; i >= 1; --i) {
    //     glm::vec3 p_prev = lidar_video.get_scan(i - 1).point_cloud().transform.position;
    //     glm::vec3 p_curr = lidar_video.get_scan(i).point_cloud().transform.position;

    //     glm::quat q_prev = glm::normalize(lidar_video.get_scan(i - 1).point_cloud().transform.rotation);
    //     glm::quat q_curr = glm::normalize(lidar_video.get_scan(i).point_cloud().transform.rotation);

    //     // Optional: avoid quaternion sign discontinuity.
    //     // q and -q represent the same rotation.
    //     if (glm::dot(q_prev, q_curr) < 0.0f) {
    //         q_curr = -q_curr;
    //     }

    //     glm::vec3 delta_position = p_curr - p_prev;

    //     // Since your update convention is:
    //     // q_new = dq * q_old
    //     glm::quat delta_rotation = glm::normalize(q_curr * glm::inverse(q_prev));

    //     lidar_video.get_scan(i).point_cloud().transform.position = delta_position;
    //     lidar_video.get_scan(i).point_cloud().transform.rotation = delta_rotation;
    // }

    // lidar_video.get_scan(0).point_cloud().transform.position = glm::vec3(0.0f);
    // lidar_video.get_scan(0).point_cloud().transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);


        
    // point_cloud_preprocessor.get_normals_from_webots_lidar_point_cloud(
    //     *lidar_video.get_scan(0).point_cloud().instance_buffer(), 
    //     lidar_video.get_scan(0).normal_buffer(), 
    //     lidar_video.get_scan(0).point_cloud().point_count());

    VoxelPointMap voxel_point_map(engine, 1500000, 1500000);
    voxel_map_reseter.reset(voxel_point_map);

    // voxel_map_inserter.insert(voxel_point_map, target_point_cloud, target_normal_buffer);
    // voxel_map_inserter.insert(voxel_point_map, lidar_video.get_scan(0).point_cloud(), lidar_video.get_scan(0).normal_buffer());
    // voxel_point_map.upload_voxels(engine, voxel_grid);

    PointCloud voxel_map_point_cloud(manager_bundle, voxel_point_map.map_point_buffer, voxel_point_map.m_map_point_count);

    sphere.set_material_data(PBRMaterialData::create(1.0f, 0.01f, skybox_exposure));
    vox_box.set_material_data(PBRMaterialData::create(0.0f, 0.95f, 1.8f, glm::vec4(1.0f), 1.0f));

    sphere.transform.position = glm::vec4(0, 2, 0, 1);

    Scene scene;

    scene.add(skybox);
    scene.add(sphere);
    // scene.add(lidar_scan);
    scene.add(scan_object);
    // scene.add(voxel_map_point_cloud);
    scene.add(line_cloud);
    
    skybox.update(scene);

    bool g_pressed = false;
    bool n_pressed = false;

    int step = 0;
    
    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();
    float timer = 0;
    uint32_t pending_skybox_environment_map_id = skybox.environment_map_id();
    bool skybox_environment_update_pending = false;
    float angular_speed = glm::half_pi<float>() * 0.5f;

    std::vector<glm::mat4> transform_mem(3);

    uint32_t received_scan_count = 0;

    while (!engine.window().should_close()) {
        engine.window().poll_events();

        if (skybox_environment_update_pending) {
            engine.device().wait_idle();
            skybox.set_environment_map_id(pending_skybox_environment_map_id);
            skybox.update(scene);
            skybox_environment_update_pending = false;
        }

        float current_frame_time = (float)glfwGetTime() - start_time;
        float delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        timer += delta_time;

        float angle = angular_speed * delta_time;
        glm::quat rot_x = glm::angleAxis(angle, glm::vec3(1.0f, 0.0f, 0.0f));
        glm::quat rot_y = glm::angleAxis(angle, glm::vec3(0.0f, 1.0f, 0.0f));

        // for (const glm::mat4 transform : transform_mem) {
        //     voxelizator.voxelize_and_submit(
        //         transparent_voxelize_prefab,
        //         vox_box.mesh_view(),
        //         offsetof(PBRVertex, position),
        //         sizeof(PBRVertex),
        //         transform,
        //         &voxel_grid.local_voxel_write_list()        
        //     );
        // }

        // for (size_t i = 0; i < transform_mem.size(); i++) {
        //     vox_box.transform.rotation = glm::normalize(
        //         vox_box.transform.rotation * rot_x * rot_y
        //     );

        //     voxelizator.voxelize_and_submit(
        //         blue_voxelize_prefab,
        //         vox_box.mesh_view(),
        //         offsetof(PBRVertex, position),
        //         sizeof(PBRVertex),
        //         vox_box.transform.get_model_matrix(),
        //         &voxel_grid.local_voxel_write_list()        
        //     );

        //     transform_mem[i] = vox_box.transform.get_model_matrix();
        // }

        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();

        // if (auto scan = scan_receiver.try_pop_scan(manager_bundle)) {
        //     network_scan = std::move(scan);

        //     LidarScan& current_scan = *network_scan;

        //     gicp_pass.fit(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer(), 10);
        //     voxel_map_inserter.insert(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
        //     voxel_grid.voxelize_point_cloud(engine, current_scan.point_cloud(), voxel_write_list, max_write_count);
        // }

        // if (auto scan = scan_receiver.try_pop_scan(manager_bundle)) {
        //     network_scan = std::move(scan);

        //     // LidarScan& current_scan = *network_scan;

        //     // renderer.render(command_buffer, current_scan.point_cloud());

        //     // gicp_pass.fit(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer(), 10);
        //     // voxel_map_inserter.insert(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
        //     // voxel_grid.voxelize_point_cloud(engine, current_scan.point_cloud(), voxel_write_list, max_write_count);
        // }


    


        // if (auto scan = scan_receiver.try_pop_scan(manager_bundle)) {
        //     if (network_scan) {
        //         retired_network_scans.push_back(std::move(network_scan));
        //     }

        //     network_scan = std::move(scan);
        //     std::cout << "Received scan #" << received_scan_count << std::endl;

        //     while (retired_network_scans.size() > engine.num_frames_in_flight()) {
        //         retired_network_scans.pop_front();
        //     }

        //     if (received_scan_count > 0)
        //         gicp_pass.fit(voxel_point_map, network_scan->point_cloud(), network_scan->normal_buffer(), 10);
            
        //     voxel_map_inserter.insert(voxel_point_map, network_scan->point_cloud(), network_scan->normal_buffer());
        //     voxel_grid.voxelize_point_cloud(engine, network_scan->point_cloud(), voxel_write_list, max_write_count);

        //     received_scan_count++;
        // }

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), window, camera);
        lighting_system.update(engine.current_frame(), window, camera);

        voxel_grid.update(window, camera);

        if (!g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_PRESS) {
            g_pressed = true;

            // uint32_t current_frame_id = lidar_video.current_frame_id();

            // if (current_frame_id > 0) {
            //     LidarScan& current_scan = lidar_video.get_scan(current_frame_id);
            //     gicp_pass.step(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
            // }

            // step++;
        }

        if (g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_RELEASE) {
            g_pressed = false;
        }

        // auto next_frame = [&]() {
        //     uint32_t current_frame_id = lidar_video.current_frame_id();

        //     if (current_frame_id > 0) {
        //         LidarScan& current_scan = lidar_video.get_scan(current_frame_id);
        //         LidarScan& previous_scan = lidar_video.get_scan(current_frame_id - 1);

        //         PointCloud& current_point_cloud = current_scan.point_cloud();
        //         PointCloud& previous_point_cloud = previous_scan.point_cloud();

        //         current_point_cloud.transform.position = previous_point_cloud.transform.position + current_point_cloud.transform.position;

        //         current_point_cloud.transform.rotation = glm::normalize(current_point_cloud.transform.rotation * previous_point_cloud.transform.rotation);

        //         // gicp_pass.fit(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer(), 10);

        //         voxel_map_inserter.insert(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
        //         voxel_grid.voxelize_point_cloud(engine, current_scan.point_cloud(), voxel_write_list, max_write_count);
        //         // voxel_map_point_cloud.set_instance_view(voxel_point_map.get_map_instance_view());
        //     }
            
        //     lidar_video.next_frame();
        // };

        if (!n_pressed && glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_PRESS) {
            n_pressed = true;
            
            // next_frame();
        }

        if (n_pressed && glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_RELEASE) {
            n_pressed = false;
        }
        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                // engine.swapchain_resources().swapchain, {{0.05f, 0.05f, 0.05f, 1.0f}});
                engine.swapchain_resources().swapchain, clear_color);
                // rgba(37, 150, 190)
                renderer.render(command_buffer, scene);

                // if (network_scan)
                //     renderer.render(command_buffer, network_scan->point_cloud(), network_scan->point_cloud().transform.get_model_matrix());

                renderer.render(command_buffer, voxel_grid.render_object());

                ui.begin_frame();
                ui.update_mouse_mode(window);

                ImGui::Begin("Debug");

                if (ImGui::Button("Next frame")) {
                    // next_frame();
                }

                ImGui::End();
                
                ui.end_frame(command_buffer);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
