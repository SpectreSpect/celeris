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
#include "renderer/point_cloud/lidar/lidar_scan_receiver.h"
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
#include "a_star/occupancy_grid_3d.h"
#include "a_star/a_star.h"
#include "a_star/a_star_structures.h"
#include "a_star/nonholonomic_a_star.h"
#include "a_star/footprint/footprint.h"
#include "a_star/footprint/footprint_visualizer.h"
#include "autopilot/spherical_pose_marker.h"
#include "autopilot/celeris.h"
#include "autopilot/celeris_visualizer.h"
#include "autopilot/vehicle_geometry.h"
#include "voxel_grid_vulkan/voxel_grid_gpu_debugger.h"
#include "camera/controllers/third_person_camera_controller.h"
#include "autopilot/vehicle_command_sender.h"
#include "vulkan_self/vulkan_submit_context.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <random>

VkClearValue clear_color = {0.05f, 0.05f, 0.05f, 1.0f};

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Celeris");
    window.set_icon(path_utils::executable_dir() / "assets" / "icon" / "celeris_icon.png");

    QueueRequest queue_request;
    queue_request.graphics_count = 2;
    queue_request.present_count = 1;
    queue_request.compute_count = 2;

    auto vehicle_config_path = []() {
        const std::filesystem::path relative_path =
            std::filesystem::path("configs") / "vehicles" / "gazelle_next_sim.yaml";

#ifdef CELERIS_SOURCE_DIR
        const std::filesystem::path source_path =
            std::filesystem::path(CELERIS_SOURCE_DIR) / relative_path;
        if (std::filesystem::exists(source_path)) {
            return source_path;
        }
#endif

        return path_utils::executable_dir() / relative_path;
    }();

    const VehicleGeometry vehicle_geometry = load_vehicle_geometry(vehicle_config_path);

    VulkanEngine engine(glfw_context, window, queue_request);

    UI ui(window, engine);
    Camera camera;
    FPSCameraController fps_camera_controller(camera);
    fps_camera_controller.speed = 20.0f;
    ThirdPersonCameraController third_person_camera_controller(camera);

    enum class CameraControllerMode {
        FPS,
        ThirdPerson
    };
    CameraControllerMode camera_controller_mode = CameraControllerMode::ThirdPerson;

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
    ManagerBundle manager_bundle(engine, shader_manager, texture_manager, material_manager, 
                                 material_instance_manager, mesh_manager, compute_pass_manager);

    VulkanSubmitContext compute_submit_context(engine.device(), engine.compute_queue(0));

    PointCloudPreprocessor point_cloud_preprocessor(
        engine.device(), 
        engine.compute_queue(),
        compute_pass_manager
    );

    LidarScanReceiver scan_receiver(point_cloud_preprocessor, 5000);
    // scan_receiver.start();

    std::unique_ptr<LidarScan> network_scan;
    std::deque<std::unique_ptr<LidarScan>> retired_network_scans;

    glm::vec3 voxel_size(1.0f);
    uint32_t vertical_inflation_size = 
        static_cast<uint32_t>(std::ceil(vehicle_geometry.size.y / voxel_size.y));
    uint32_t horizontal_inflation_size = 
        static_cast<uint32_t>(std::ceil((vehicle_geometry.size.x / voxel_size.x) / 2.0f));
    glm::ivec3 chunk_size(16);
    VoxelGrid::VoxelGridDesc voxel_grid_desc {
        .chunk_size = chunk_size,
        .voxel_size = voxel_size,
        .count_active_chunks = 15'000,
        .max_quads = 2'500'000,
        .chunk_hash_table_size_factor = 1.0f,
        .count_evict_buckets = 32,
        .min_free_chunks = 4'500,
        .tomb_fraction_to_rebuild = 0.2f,
        .eviction_bucket_shell_thickness = chunk_size.x * voxel_size.x * 1,
        .mean_count_quads_in_chunk = 200,
        .allocation_retry_list_size = 10'000, // Не так много занимает, в целом можно не жмотничать
        .buddy_allocator_nodes_factor = 1.0,
        .render_distance = chunk_size.x * voxel_size.x * 30,
        .generation_distance = 5,
        .max_write_count = chunk_size.x * chunk_size.y * chunk_size.z * static_cast<uint32_t>(2'000),
        .inflation_size = 5u,
        .car_height_voxels = 3u,
        .negative_x_inflation_size = horizontal_inflation_size,
        .positive_x_inflation_size = horizontal_inflation_size,
        .negative_y_inflation_size = vertical_inflation_size,
        .positive_y_inflation_size = 0u,
        .negative_z_inflation_size = horizontal_inflation_size,
        .positive_z_inflation_size = horizontal_inflation_size,
        .display_inflated_voxels = 0u,
        // .inflated_voxel_color = 0xFF0707FFu, // 0xFF3355FFu
        .inflated_voxel_color = 0xFFFFFFFFu,
    };

    VoxelGrid voxel_grid(
        engine.physical_device(),
        engine.device(),
        engine.compute_queue(),
        compute_pass_manager,
        material_instance_manager,
        voxel_grid_desc,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    bool display_inflated_voxels = voxel_grid.params().display_inflated_voxels != 0u;
    float inflated_voxel_color[4] = {
        float((voxel_grid.params().inflated_voxel_color >> 24u) & 0xFFu) / 255.0f,
        float((voxel_grid.params().inflated_voxel_color >> 16u) & 0xFFu) / 255.0f,
        float((voxel_grid.params().inflated_voxel_color >> 8u) & 0xFFu) / 255.0f,
        float(voxel_grid.params().inflated_voxel_color & 0xFFu) / 255.0f
    };
    int negative_x_inflation_size = static_cast<int>(voxel_grid.params().negative_x_inflation_size);
    int positive_x_inflation_size = static_cast<int>(voxel_grid.params().positive_x_inflation_size);
    int negative_y_inflation_size = static_cast<int>(voxel_grid.params().negative_y_inflation_size);
    int positive_y_inflation_size = static_cast<int>(voxel_grid.params().positive_y_inflation_size);
    int negative_z_inflation_size = static_cast<int>(voxel_grid.params().negative_z_inflation_size);
    int positive_z_inflation_size = static_cast<int>(voxel_grid.params().positive_z_inflation_size);
    auto pack_inflated_voxel_color = [](const float color[4]) -> uint32_t {
        auto pack_channel = [](float value) -> uint32_t {
            return static_cast<uint32_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
        };

        return (pack_channel(color[0]) << 24u) |
               (pack_channel(color[1]) << 16u) |
               (pack_channel(color[2]) << 8u) |
               pack_channel(color[3]);
    };

    VoxelGridGPUDebugger debugger(
        voxel_grid,
        engine.device(),
        engine.compute_queue(),
        window,
        camera,
        true
    );

    Voxelizator::VoxelizatorDesc voxelizator_desc {
        .chunk_size = chunk_size,
        .voxel_size = voxel_size,
        .counter_hash_table_size = 150'000,
        .count_hash_table_failure_slots = 150'000,
        .count_voxel_writes = 0, // Будут использоваться те, что внутри voxel_grid
        .count_hash_table_attempts = 5
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
    uint32_t scan_index_count = mesher.convert_to_mesh<PBRVertex, PointInstance>(
        lidar_scan.point_cloud(),
        scan_vertex_buffer,
        scan_index_buffer
    );

    MeshView scan_mesh_view(
        scan_vertex_buffer.get_view(),
        scan_index_buffer.get_view(),
        scan_index_count
    );

    RenderObject scan_object(scan_mesh_view, material_instance_manager.pbr);
    // RenderObject scan_object(mesh_manager.cube.get_view(), material_instance_manager.pbr);
    scan_object.set_material_data(PBRMaterialData::create(0.0f, 0.95f, 1.8f, glm::vec4(1.0f), 1.0f));

    scan_object.transform.scale = glm::vec3(5.0f);

    // voxelizator.voxelize<PBRVertex>(
    //     blue_voxelize_prefab,
    //     scan_object.mesh_view(),
    //     scan_object.transform.get_model_matrix(),
    //     &voxel_grid.local_voxel_write_list()
    // );

    glm::ivec2 wall_junction_xz = glm::ivec2(5, 5);
    std::vector<VoxelWriteGPU> test_voxel_writes;
    test_voxel_writes.reserve(50);

    auto add_test_wall = [&](glm::ivec3 wall_origin, glm::ivec3 wall_size) {
        for (int x = 0; x < wall_size.x; x++)
            for (int y = 0; y < wall_size.y; y++)
                for (int z = 0; z < wall_size.z; z++) {
                    glm::ivec3 world_voxel = wall_origin + glm::ivec3(x, y, z);

                    if (glm::ivec2(world_voxel.x, world_voxel.z) == wall_junction_xz) {
                        continue;
                    }

                    glm::ivec3 base_color{0, 98, 255};
                    glm::ivec3 color = glm::vec3(base_color) * glm::vec3(0.5 + math_utils::dist(math_utils::rng) * 0.5);

                    test_voxel_writes.push_back(
                        VoxelWriteGPU{
                            .world_voxel = glm::ivec4(world_voxel, 0),
                            .voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, color),
                            .set_flags = OVERWRITE_BIT
                        }
                    );
                }
    };

    add_test_wall(glm::ivec3(5, 0, 0), glm::ivec3(1, 5, 20));
    add_test_wall(glm::ivec3(13, 0, 0), glm::ivec3(1, 5, 20));
    // add_test_wall(glm::ivec3(5, 0, 5), glm::ivec3(6, 5, 1));

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

    // voxel_grid.update(window, camera);


    // VoxelGridChunk chunk = voxel_grid.read_chunk(glm::ivec3(0, 0, 0));
    // glm::uvec3 read_chunk_size = chunk.chunk_size();

    // for (int x = 0; x < read_chunk_size.x; x++)
    //     for (int y = 0; y < read_chunk_size.y; y++)
    //         for (int z = 0; z < read_chunk_size.z; z++) {
    //             VoxelDataGPU voxel = chunk.voxel(glm::uvec3(x, y, z));
    //             glm::vec4 color = voxel.color_vec4();

    //             if (color.x != 0 || color.y != 0 || color.z != 0)
    //                 std::cout << "pos: (" << x << ", " << y << ", " << z << ")" << std::endl;
    //         }



    uint32_t max_write_count = 100000;
    VulkanBuffer voxel_write_list = VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t) * 4 + sizeof(VoxelWriteGPU) * max_write_count);

    Renderer renderer(engine, frame_resources);

    const float skybox_exposure = 1.8f;
    GazelleNext gazelle(mesh_manager, material_instance_manager, vehicle_geometry, skybox_exposure);

    Skybox skybox(
        mesh_manager.skybox_cube,
        material_instance_manager.st_peters_square_night_4k_hdr,
        texture_manager,
        material_manager.pbr_mp,
        TextureManager::st_peters_square_night_4k_pbr_map_id,
        skybox_exposure
    );

    bool has_start_pos = true;
    bool has_end_pos = true;
    bool has_planned_path = false;

    VulkanSubmitContext planner_submit_context(
        engine.device(),
        engine.compute_queue(1)
    );

    Celeris celeris(
        engine,
        engine.compute_queue(),
        compute_submit_context,
        manager_bundle, 
        material_instance_manager,
        voxel_grid, 
        voxelizator,
        scan_vertex_buffer,
        scan_index_buffer,
        mesher,
        Celeris::CelerisDesc{
            .vehicle_geometry = vehicle_geometry,
            .footprint_sample_count = 5,
            .footprint_horizontal_inflation_size = horizontal_inflation_size,
            .footprint_vertical_inflation_size = vertical_inflation_size
        }
    );
    celeris.set_start(NonholonomicPos{.pos = glm::vec3(0.0f, 1.5f, 0.0f)});
    // celeris.set_goal(NonholonomicPos{.pos = glm::vec3(-170.69f, 1.92f, -51.30f)});
    celeris.set_goal(NonholonomicPos{.pos = glm::vec3(5, 1, 5)});
    // celeris.set_goal(NonholonomicPos{.pos = glm::vec3(-170.69f, 1.5f, -0.0f)});
    celeris.start(std::move(planner_submit_context));

    CelerisVisualizer celeris_visualizer(mesh_manager, 
                                         material_instance_manager, 
                                         celeris, 
                                         vehicle_geometry,
                                         20000, 
                                         skybox_exposure);

    auto use_fps_camera_controller = [&]() {
        if (camera_controller_mode == CameraControllerMode::FPS)
            return;

        const glm::vec3 front = glm::normalize(camera.front);
        fps_camera_controller.yaw = glm::degrees(std::atan2(front.z, front.x));
        fps_camera_controller.pitch = glm::degrees(std::asin(glm::clamp(front.y, -1.0f, 1.0f)));
        fps_camera_controller.first_mouse = true;
        camera_controller_mode = CameraControllerMode::FPS;
    };

    auto use_third_person_camera_controller = [&]() {
        camera_controller_mode = CameraControllerMode::ThirdPerson;
    };

    float car_speed = celeris.car_speed();

    auto start_path_planning = [&]() {
        if (!has_start_pos || !has_end_pos)
            return;

        celeris.request_path_replan();
    };

    auto make_pose_from_camera = [&](NonholonomicPos& out_pose) {
        glm::vec3 horizontal_front(camera.front.x, 0.0f, camera.front.z);
        if (glm::dot(horizontal_front, horizontal_front) == 0.0f)
            return false;

        out_pose.pos = camera.position;
        out_pose.theta = std::atan2(horizontal_front.z, horizontal_front.x);

        return celeris.adjust_to_ground(out_pose.pos);
    };

    auto place_start = [&]() {
        NonholonomicPos pose;
        if (make_pose_from_camera(pose)) {
            celeris.set_start(pose);
            has_start_pos = true;
            has_planned_path = false;
            start_path_planning();
        }
    };

    auto place_end = [&]() {
        NonholonomicPos pose;
        if (make_pose_from_camera(pose)) {
            celeris.set_goal(pose);
            has_end_pos = true;
            has_planned_path = false;
            start_path_planning();
        }
    };

    // LidarVideo lidar_video(
    //     manager_bundle, 
    //     point_cloud_preprocessor, 
    //     "/home/spectre/TEMP_lidar_output_mesh/recording_16/index.csv", 
    //     0,
    //     2
    // );

    // for (int i = static_cast<int>(lidar_video.get_scan_count()) - 1; i >= 1; --i) {
    //     glm::vec3 p_prev = lidar_video.get_scan(i - 1).point_cloud().transform.position;
    //     glm::vec3 p_curr = lidar_video.get_scan(i).point_cloud().transform.position;

    //     glm::quat q_prev = glm::normalize(lidar_video.get_scan(i - 1).point_cloud().transform.rotation);
    //     glm::quat q_curr = glm::normalize(lidar_video.get_scan(i).point_cloud().transform.rotation);

    //     if (glm::dot(q_prev, q_curr) < 0.0f) {
    //         q_curr = -q_curr;
    //     }

    //     glm::vec3 delta_position = p_curr - p_prev;
    //     glm::quat delta_rotation = glm::normalize(q_curr * glm::inverse(q_prev));

    //     lidar_video.get_scan(i).point_cloud().transform.position = delta_position;
    //     lidar_video.get_scan(i).point_cloud().transform.rotation = delta_rotation;
    // }

    // lidar_video.get_scan(0).point_cloud().transform.position = glm::vec3(0.0f);
    // lidar_video.get_scan(0).point_cloud().transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);

    LidarScan target_scan(manager_bundle, point_cloud_preprocessor, "assets/lidar_scans/frame_000000.bin");
    LidarScan source_scan(manager_bundle, point_cloud_preprocessor, "assets/lidar_scans/frame_000000.bin");

    target_scan.point_cloud().set_color(glm::vec4(0, 0, 1, 1));
    source_scan.point_cloud().set_color(glm::vec4(1, 0, 0, 1));

    source_scan.point_cloud().transform.position += glm::vec3(-2, 2, -2);

    // celeris.voxel_map_reseter().reset(celeris.voxel_point_map());
    // celeris.voxel_map_point_inserter().insert(
    //     celeris.voxel_point_map(), 
    //     lidar_video.get_scan(0).point_cloud(), 
    //     lidar_video.get_scan(0).normal_buffer()
    // );

    PointCloud voxel_point_map(
        manager_bundle, 
        celeris.voxel_point_map().map_point_buffer, 
        celeris.voxel_point_map().m_map_point_count
    );
    voxel_point_map.set_color(glm::vec4(0, 0, 0, 1));
    uint32_t rendered_celeris_scan_count = celeris.received_scan_count();

    // auto process_current_lidar_frame = [&]() {
    //     uint32_t current_frame_id = lidar_video.current_frame_id();

    //     if (current_frame_id > 0) {
    //         LidarScan& current_scan = lidar_video.get_scan(current_frame_id);
    //         LidarScan& previous_scan = lidar_video.get_scan(current_frame_id - 1);

    //         PointCloud& current_point_cloud = current_scan.point_cloud();
    //         PointCloud& previous_point_cloud = previous_scan.point_cloud();

    //         current_point_cloud.transform.position =
    //             previous_point_cloud.transform.position + current_point_cloud.transform.position;
    //         current_point_cloud.transform.rotation =
    //             glm::normalize(current_point_cloud.transform.rotation * previous_point_cloud.transform.rotation);

    //         celeris.gicp_pass().fit(
    //             celeris.voxel_point_map(),
    //             current_scan.point_cloud(),
    //             current_scan.normal_buffer(),
    //             10
    //         );

    //         celeris.voxel_map_point_inserter().insert(
    //             celeris.voxel_point_map(),
    //             current_scan.point_cloud(),
    //             current_scan.normal_buffer()
    //         );

    //         voxel_point_map.set_instance_view(celeris.voxel_point_map().get_map_instance_view());
    //     }

    //     lidar_video.next_frame();
    // };
    FootprintVisualizer footprint_visualizer(
        mesh_manager,
        material_instance_manager,
        celeris,
        celeris.footprint(),
        skybox_exposure
    );

    auto place_footprint = [&]() {
        NonholonomicPos pose;
        if (make_pose_from_camera(pose)) {
            footprint_visualizer.update_footprint(pose);
        }
    };

    Scene scene;

    scene.add(skybox);

    scene.add(celeris_visualizer);
    scene.add(footprint_visualizer);
    // scene.add(gazelle);
    // scene.add(target_scan);
    // scene.add(voxel_point_map);
    // scene.add(source_scan);

    // scene.add(lidar_video);
    
    skybox.update(scene);

    bool g_pressed = false;
    bool n_pressed = false;
    bool place_start_pressed = false;
    bool place_end_pressed = false;
    bool start_path_planning_pressed = false;
    bool place_footprint_pressed = false;
    bool fps_camera_pressed = false;
    bool third_person_camera_pressed = false;

    int step = 0;
    
    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();
    float timer = 0;
    uint32_t pending_skybox_environment_map_id = skybox.environment_map_id();
    bool skybox_environment_update_pending = false;
    float angular_speed = glm::half_pi<float>() * 0.5f;

    std::vector<glm::mat4> transform_mem(3);
    std::vector<NonholonomicPos> car_playback_path;
    bool car_playback_active = false;
    bool car_playback_loop = false;
    float car_playback_distance = 0.0f;
    float car_playback_total_length = 0.0f;
    float car_playback_speed = 4.0f;

    auto path_length = [](const std::vector<NonholonomicPos>& path) {
        float length = 0.0f;
        for (size_t i = 1; i < path.size(); i++) {
            length += glm::distance(path[i - 1].pos, path[i].pos);
        }

        return length;
    };

    auto sample_path_pose = [](const std::vector<NonholonomicPos>& path, float target_distance) {
        if (path.empty())
            return NonholonomicPos{};

        if (path.size() == 1)
            return path.front();

        float traversed_distance = 0.0f;
        for (size_t i = 1; i < path.size(); i++) {
            const NonholonomicPos& from = path[i - 1];
            const NonholonomicPos& to = path[i];
            const float segment_length = glm::distance(from.pos, to.pos);

            if (segment_length <= 0.0f)
                continue;

            if (traversed_distance + segment_length >= target_distance) {
                const float t = glm::clamp(
                    (target_distance - traversed_distance) / segment_length,
                    0.0f,
                    1.0f
                );

                NonholonomicPos pose = from;
                pose.pos = glm::mix(from.pos, to.pos, t);
                pose.theta = from.theta + NonholonomicAStar::angle_diff(from.theta, to.theta) * t;
                pose.steer = to.steer;
                pose.dir = to.dir;
                pose.dubins_segment_id = to.dubins_segment_id;
                return pose;
            }

            traversed_distance += segment_length;
        }

        return path.back();
    };

    auto start_car_playback = [&]() {
        car_playback_path = celeris.path_result_snapshot().nonholonomic_astar_path;
        car_playback_total_length = path_length(car_playback_path);
        car_playback_distance = 0.0f;
        car_playback_active = car_playback_path.size() >= 2 && car_playback_total_length > 0.0f;

        if (car_playback_active) {
            celeris_visualizer.set_car_pose_override(car_playback_path.front());
        }
    };

    use_fps_camera_controller();

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
        //     voxelizator.voxelize_and_submit<PBRVertex>(
        //         transparent_voxelize_prefab,
        //         vox_box.mesh_view(),
        //         transform,
        //         &voxel_grid.local_voxel_write_list()        
        //     );
        // }

        // for (size_t i = 0; i < transform_mem.size(); i++) {
        //     vox_box.transform.rotation = glm::normalize(
        //         vox_box.transform.rotation * rot_x * rot_y
        //     );

        //     voxelizator.voxelize_and_submit<PBRVertex>(
        //         blue_voxelize_prefab,
        //         vox_box.mesh_view(),
        //         vox_box.transform.get_model_matrix(),
        //         &voxel_grid.local_voxel_write_list()        
        //     );

        //     transform_mem[i] = vox_box.transform.get_model_matrix();
        // }

        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();


        // celeris.update();
        celeris.update(compute_submit_context);
        if (rendered_celeris_scan_count != celeris.received_scan_count()) {
            voxel_point_map.set_instance_view(celeris.voxel_point_map().get_map_instance_view());
            rendered_celeris_scan_count = celeris.received_scan_count();
        }

        if (car_playback_active) {
            car_playback_distance += car_playback_speed * delta_time;

            if (car_playback_distance >= car_playback_total_length) {
                if (car_playback_loop) {
                    car_playback_distance = std::fmod(car_playback_distance, car_playback_total_length);
                } else {
                    car_playback_distance = car_playback_total_length;
                    car_playback_active = false;
                }
            }

            celeris_visualizer.set_car_pose_override(
                sample_path_pose(car_playback_path, car_playback_distance)
            );
        }

        celeris_visualizer.update();

        if (!fps_camera_pressed && glfwGetKey(window.handle(), GLFW_KEY_F) == GLFW_PRESS) {
            fps_camera_pressed = true;
            use_fps_camera_controller();
        }
        if (fps_camera_pressed && glfwGetKey(window.handle(), GLFW_KEY_F) == GLFW_RELEASE)
            fps_camera_pressed = false;

        if (!third_person_camera_pressed && glfwGetKey(window.handle(), GLFW_KEY_R) == GLFW_PRESS) {
            third_person_camera_pressed = true;
            use_third_person_camera_controller();
        }
        if (third_person_camera_pressed && glfwGetKey(window.handle(), GLFW_KEY_R) == GLFW_RELEASE)
            third_person_camera_pressed = false;

        third_person_camera_controller.set_target(celeris_visualizer.get_start_marker_pos());
        if (camera_controller_mode == CameraControllerMode::FPS)
            fps_camera_controller.update(window, delta_time);
        else
            third_person_camera_controller.update(window, delta_time);

        frame_resources.update_camera(engine.current_frame(), window, camera);

        lighting_system.update(engine.current_frame(), window, camera);

        voxel_grid.update(window, camera);        

        if (!place_start_pressed && glfwGetKey(window.handle(), GLFW_KEY_1) == GLFW_PRESS) {
            place_start_pressed = true;
            place_start();
        }

        if (place_start_pressed && glfwGetKey(window.handle(), GLFW_KEY_1) == GLFW_RELEASE) {
            place_start_pressed = false;
        }

        if (!place_end_pressed && glfwGetKey(window.handle(), GLFW_KEY_2) == GLFW_PRESS) {
            place_end_pressed = true;
            place_end();
        }

        if (place_end_pressed && glfwGetKey(window.handle(), GLFW_KEY_2) == GLFW_RELEASE) {
            place_end_pressed = false;
        }

        if (!start_path_planning_pressed && glfwGetKey(window.handle(), GLFW_KEY_3) == GLFW_PRESS) {
            start_path_planning_pressed = true;
            start_path_planning();
        }

        if (start_path_planning_pressed && glfwGetKey(window.handle(), GLFW_KEY_3) == GLFW_RELEASE) {
            start_path_planning_pressed = false;
        }

        if (!place_footprint_pressed && glfwGetKey(window.handle(), GLFW_KEY_4) == GLFW_PRESS) {
            place_footprint_pressed = true;
            place_footprint();
        }

        if (place_footprint_pressed && glfwGetKey(window.handle(), GLFW_KEY_4) == GLFW_RELEASE) {
            place_footprint_pressed = false;
        }

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

        if (!n_pressed && glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_PRESS) {
            n_pressed = true;
            // process_current_lidar_frame();
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
                
                // {
                //     VulkanCommandBuffer& debugger_command_buffer = debugger.command_buffer();
                //     auto scope = debugger_command_buffer.begin_scope();
                //     debugger.dispay_debug_window(camera);
                //     debugger.display_build_from_dirty_window(debugger_command_buffer);
                //     debugger.display_build_cmd_window(debugger_command_buffer, window, camera);
                //     debugger.display_draw_pipline_window(debugger_command_buffer);
                //     debugger.display_chunk_eviction_window(debugger_command_buffer, camera);
                //     debugger.display_stream_chunks_pipeline_window(debugger_command_buffer, camera);
                //     debugger.display_hash_table_window();
                // }
                // debugger.submit_commands();
                

                ImGui::Begin("Debug");

                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

                ImGui::TextUnformatted("Camera position:");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "x: %.2f", camera.position.x);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "y: %.2f", camera.position.y);
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.0f, 0.35f, 1.0f, 1.0f), "z: %.2f", camera.position.z);

                if (ImGui::CollapsingHeader("Voxel grid debug")) {
                    bool inflated_settings_changed = false;
                    inflated_settings_changed |= ImGui::Checkbox(
                        "Display inflated voxels",
                        &display_inflated_voxels
                    );
                    inflated_settings_changed |= ImGui::ColorEdit4(
                        "Inflated voxel color",
                        inflated_voxel_color,
                        ImGuiColorEditFlags_AlphaBar
                    );

                    inflated_settings_changed |= ImGui::SliderInt(
                        "Negative X inflation size",
                        &negative_x_inflation_size,
                        0,
                        static_cast<int>(voxel_grid.params().chunk_size.x)
                    );
                    inflated_settings_changed |= ImGui::SliderInt(
                        "Positive X inflation size",
                        &positive_x_inflation_size,
                        0,
                        static_cast<int>(voxel_grid.params().chunk_size.x)
                    );
                    inflated_settings_changed |= ImGui::SliderInt(
                        "Negative Y inflation size",
                        &negative_y_inflation_size,
                        0,
                        static_cast<int>(voxel_grid.params().chunk_size.y)
                    );
                    inflated_settings_changed |= ImGui::SliderInt(
                        "Positive Y inflation size",
                        &positive_y_inflation_size,
                        0,
                        static_cast<int>(voxel_grid.params().chunk_size.y)
                    );
                    inflated_settings_changed |= ImGui::SliderInt(
                        "Negative Z inflation size",
                        &negative_z_inflation_size,
                        0,
                        static_cast<int>(voxel_grid.params().chunk_size.z)
                    );
                    inflated_settings_changed |= ImGui::SliderInt(
                        "Positive Z inflation size",
                        &positive_z_inflation_size,
                        0,
                        static_cast<int>(voxel_grid.params().chunk_size.z)
                    );

                    if (inflated_settings_changed) {
                        voxel_grid.set_inflated_voxel_debug_display(
                            display_inflated_voxels ? 1u : 0u,
                            pack_inflated_voxel_color(inflated_voxel_color),
                            static_cast<uint32_t>(negative_x_inflation_size),
                            static_cast<uint32_t>(positive_x_inflation_size),
                            static_cast<uint32_t>(negative_y_inflation_size),
                            static_cast<uint32_t>(positive_y_inflation_size),
                            static_cast<uint32_t>(negative_z_inflation_size),
                            static_cast<uint32_t>(positive_z_inflation_size)
                        );
                    }
                }

                celeris_visualizer.display_debug_controls();
                celeris.display_path_planner_debug_controls();
                
                // ImGui::TextColored(ImVec4(1.0f, 0.35f, 1.0f, 1.0f), "Current frame id: %d", lidar_video.current_frame_id());

                // if (ImGui::Button("FPS controller")) {
                //     use_fps_camera_controller();
                // }
                // ImGui::SameLine();
                // ImGui::TextUnformatted("Key: F");

                // if (ImGui::Button("Third person controller")) {
                //     use_third_person_camera_controller();
                // }
                // ImGui::SameLine();
                // ImGui::TextUnformatted("Key: R");

                if (ImGui::CollapsingHeader("Path planning controls")) {
                    if (ImGui::Button("Place start")) {
                        place_start();
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted("Key: 1");

                    if (ImGui::Button("Place end")) {
                        place_end();
                    }
                    
                    ImGui::SameLine();
                    ImGui::TextUnformatted("Key: 2");

                    if (ImGui::Button("Start path planning")) {
                        start_path_planning();
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted("Key: 3");

                    if (ImGui::Button("Place footprint")) {
                        place_footprint();
                    }
                    ImGui::SameLine();
                    ImGui::TextUnformatted("Key: 4");

                    if (ImGui::Button("Play car path")) {
                        start_car_playback();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop")) {
                        car_playback_active = false;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Reset car")) {
                        car_playback_active = false;
                        celeris_visualizer.clear_car_pose_override();
                    }

                    ImGui::Checkbox("Loop car path", &car_playback_loop);
                    ImGui::SliderFloat("Car playback speed", &car_playback_speed, 0.1f, 20.0f, "%.1f m/s");

                }

                // if (ImGui::InputFloat("Celeris car speed", &car_speed, 1.0f, 10.0f, "%.1f")) {
                //     celeris.set_car_speed(car_speed);
                // }

                ImGui::End();
                
                ui.end_frame(command_buffer);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
