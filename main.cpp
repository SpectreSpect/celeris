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
#include "renderer/point_cloud/gicp/gicp_pass.h"
#include "renderer/point_cloud/gicp/voxel_point_map.h"
#include "renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "imgui_layer.h"
#include "renderer/lighting_system/lighting_system.h"
#include "renderer/pbr/equirect_to_cubemap_pass.h"
#include "renderer/pbr/brdf_lut_pass.h"
#include "renderer/pbr/prefilter_map_pass.h"
#include "renderer/pbr/irradiance_map_pass.h"
#include "renderer/mcp/mcp_visualization_texture_pass.h"
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
#include "autopilot/gamepad_controller.h"
#include "autopilot/vehicle_geometry.h"
#include "voxel_grid_vulkan/voxel_grid_gpu_debugger.h"
#include "camera/controllers/third_person_camera_controller.h"
#include "autopilot/vehicle_command_sender.h"
#include "autopilot/celeris_user_controller.h"
#include "vulkan_self/vulkan_submit_context.h"
#include "vulkan_self/keyboard_input_reciever.h"
#include "autopilot/arrow.h"
#include "autopilot/sensors/imu/imu_receiver.h"
#include "autopilot/sensors/imu/imu_measurement.h"
#include "autopilot/odometry/odometry_estimator.h"
#include "autopilot/sensors/lidar/lidar_scan.h"
#include "autopilot/sensors/lidar/lidar_scan_receiver.h"
#include "autopilot/sensors/lidar/lidar_message.h"
#include "autopilot/recorder/lidar_message_odometry_recording.h"
#include "autopilot/sensors/lidar/deskewing/lidar_scan_deskewer.h"
#include "autopilot/sensors/lidar/deskewing/deskewing_debugger.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <vector>
#include <random>
#include <string>

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

    glm::vec3 voxel_size(0.2f);
    // glm::vec3 voxel_size(1.0f);
    uint32_t vertical_inflation_size =
        static_cast<uint32_t>(std::ceil(vehicle_geometry.size.y / voxel_size.y));
    uint32_t horizontal_inflation_size =
        static_cast<uint32_t>(std::ceil((vehicle_geometry.size.x / voxel_size.x) / 2.0f));
    // uint32_t vertical_inflation_size = 1;
    // uint32_t horizontal_inflation_size = 1;
    glm::ivec3 chunk_size(16);
    VoxelGrid::VoxelGridDesc voxel_grid_desc {
        .chunk_size = chunk_size,
        .voxel_size = voxel_size,
        .count_active_chunks = 40'000,
        .max_quads = 8'000'000,
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
        .inflated_curvature_limit_exceeded_voxel_color = 0xFF0707FFu,
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

    PointCloudMesher mesher(
        engine.physical_device(),
        engine.device(),
        engine.compute_queue(),
        compute_pass_manager,
        3600
    );

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

    VulkanSubmitContext planner_submit_context(
        engine.device(),
        engine.compute_queue(1)
    );

    // PointCloud loaded_point_cloud(manager_bundle, "/home/spectre/TEMP_lidar_output_mesh/test_lidar_scan.bin");
    // LidarScan loaded_lidar_scan(manager_bundle, "/home/spectre/TEMP_lidar_output_mesh/test_lidar_scan.lsb");

    // LidarMessage loaded_lidar_msg("/home/spectre/TEMP_lidar_output_mesh/test_lidar_msg.lmb");
    // LidarScan loaded_lidar_scan(manager_bundle, point_cloud_preprocessor, loaded_lidar_msg);

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
        gazelle,
        Celeris::CelerisDesc{
            .vehicle_geometry = vehicle_geometry,
            .footprint_sample_count = 5,
            .footprint_horizontal_inflation_size = horizontal_inflation_size,
            .footprint_vertical_inflation_size = vertical_inflation_size,
            .gamepad_commands_enabled = false
        }
    );
    celeris.odometry_estimator().set_gravity(glm::vec3(-0.203579f, 10.0965f, -0.240282f)); // ros bag 2
    // celeris.odometry_estimator().set_gravity(glm::vec3(-0.123099f, 9.78485f, -0.69118f)); // simulator
    // celeris.odometry_estimator().start_gravity_calibration(100);
    // celeris.start_lidar_recording("/home/spectre/TEMP_lidar_output_mesh/ros_bag_recording_TEMP_DELETE_THIS/");

    // DeskewingDebugger deskewing_debugger(
    //     manager_bundle, 
    //     "/home/spectre/TEMP_lidar_output_mesh/ros_bag_recording_TEMP_DELETE_THIS/",
    //     3277
    // );

    // celeris.set_vehicle_command(VehicleCommand{
    //     .acceleration = 0.5f,
    //     .steering_angle_velocity = 0.5f
    // });

    const NonholonomicPos start_lidar_scan_position{.pos = glm::vec3(0.0f, 0.0f, 0.0f)};
    // celeris.set_start(start_lidar_scan_position);
    celeris.set_goal(NonholonomicPos{.pos = glm::vec3(5, 1, 5)});
    // has_end_pos = true;
    // celeris.set_start_lidar_scan_position(start_lidar_scan_position);
    // celeris.load_map(saved_maps_directory / "test4.vpm");
    // celeris.load_waypoint_path(saved_waypoint_paths_directory / "robocross_sim_3.wpp");
    // celeris.start(std::move(planner_submit_context));
    // celeris.set_vehicle_command(VehicleCommand{
    //     .acceleration = 0.5f,
    //     .steering_angle_velocity = 0.5f
    // });

    CelerisVisualizer celeris_visualizer(
        mesh_manager,
        material_instance_manager,
        celeris,
        vehicle_geometry,
        20000,
        skybox_exposure
    );

    Transform quad_transform;
    quad_transform.scale = glm::vec3(10.0f, 1.0f, 10.0f);

    McpVisualizationTexturePass mcp_visualization_texture_pass(engine, compute_pass_manager);
    VulkanTexture2D mcp_visualization_texture = mcp_visualization_texture_pass.generate(
        512,
        512,
        quad_transform.get_model_matrix(),
        camera.position,
        5.0f
    );

    SlotPassInstance quad_pi(material_manager.create_pbr_material(
            engine, 
            texture_manager.irradiance_maps, 
            texture_manager.prefilter_maps, 
            texture_manager.brdf_lut,
            mcp_visualization_texture
        ));

    RenderObject quad_object(mesh_manager.quad, quad_pi);
    quad_object.set_material_data(PBRMaterialData::create(0, 1.0f, skybox_exposure, glm::vec4(1, 1, 1, 1)));
    quad_object.transform = quad_transform;

    Arrow test_arrow(
        engine,
        mesh_manager,
        material_instance_manager,
        glm::vec4(1, 0, 0, 1),
        10.0f,                          // length
        5.0f,                           // line width in pixels
        glm::vec3(0.0f, 3.0f, 0.0f),  // position above ground
        glm::vec3(1.0f, 0.0f, 0.0f),  // direction
        2.5f,                           // wing length
        glm::radians(30.0f)             // wing angle to the main line
    );

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;

    // GazelleNext test_gazelle_next(mesh_manager, material_instance_manager, vehicle_geometry, skybox_exposure);

    Scene scene;

    scene.add(skybox);
    // scene.add(celeris_visualizer);
    // // scene.add(test_gazelle_next);
    // scene.add(voxel_grid.render_object());
    scene.add(quad_object);
    // scene.add(test_arrow);

    // scene.add(deskewing_debugger);

    skybox.update(scene);

    GamepadController gamepad_controller;

    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();
    float timer = 0;
    uint32_t pending_skybox_environment_map_id = skybox.environment_map_id();
    bool skybox_environment_update_pending = false;
    float angular_speed = glm::half_pi<float>() * 0.5f;

    int current_entry_id = 0;

    // use_fps_camera_controller();

    KeyboardInputReciever keyboard_input_reciever(window);

    CelerisUserController celeris_user_controller(celeris, celeris_visualizer);

    while (!engine.window().should_close()) {
        engine.window().poll_events();
        keyboard_input_reciever.update();
        gamepad_controller.update(
            celeris.car_speed(),
            celeris.vehicle_speed(),
            celeris.vehicle_steering_angle()
        );
        
        celeris.set_gamepad_command(gamepad_controller.command());

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

        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();
        
        celeris.update(compute_submit_context);
        celeris_user_controller.update(delta_time, camera, keyboard_input_reciever, fps_camera_controller);
        celeris_visualizer.update();
        // deskewing_debugger.update(keyboard_input_reciever);
        
        third_person_camera_controller.set_target(celeris.vehicle_transform().position);
        if (celeris_user_controller.camera_controller_mode() == CelerisUserController::CameraControllerMode::FPS)
            fps_camera_controller.update(window, delta_time);
        else
            third_person_camera_controller.update(window, delta_time);

        frame_resources.update_camera(engine.current_frame(), window, camera);

        lighting_system.update(engine.current_frame(), window, camera);

        voxel_grid.render_object().visible = celeris_user_controller.show_voxel_grid();
        voxel_grid.update(window, camera);

        mcp_visualization_texture_pass.render(
            mcp_visualization_texture,
            quad_transform.get_model_matrix(),
            camera.position,
            5.0f
        );

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain, clear_color);
                renderer.render(command_buffer, scene);

                ui.begin_frame();
                ui.update_mouse_mode(window);

                celeris_user_controller.display_interface(camera, gamepad_controller);

                ui.end_frame(command_buffer);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
