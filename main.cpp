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
#include "vulkan_self/pipeline/pipeline_pass_builder.h"
#include "vulkan_self/compute/compute_pass.h"
#include "vulkan_self/image/vulkan_image.h"
#include "vulkan_self/image/cpu_image.h"
#include "vulkan_self/image/vulkan_texture_2d.h"
#include "path_utils.h"
#include "vulkan_self/material/material_system.h"
#include "vulkan_self/material/material_instance.h"
#include "renderer/resources/frame_resources.h"
#include "camera/camera.h"
#include "camera/controllers/fps_camera_controller.h"
#include "renderer/transform.h"
#include "renderer/mesh.h"
#include "renderer/render_object.h"
#include "renderer/transform_push_constants.h"
#include "vulkan_self/material/blin_phong_material_pass.h"
#include "vulkan_self/material/material_instance_temp.h"
#include "vulkan_self/material/blinn_phong_material_instance.h"
#include "vulkan_self/material/unlit_material_instance.h"
#include "vulkan_self/material/material_instance.h"
#include "renderer/shader_manager.h"
#include "renderer/material_manager.h"
#include "vulkan_self/compute/compute_pass_builder.h"
#include "renderer/compute_pass_manager.h"
#include "renderer/compute_pass_instance.h"
#include "vulkan_self/material/material_buffer.h"
#include "renderer/instanced_render_object.h"
#include "renderer/instance_batch.h"
#include "renderer/texture_manager.h"
#include "renderer/material_instance_manager.h"
#include "renderer/point_cloud/point_instance.h"
#include "renderer/renderer.h"
#include "renderer/mesh_manager.h"
#include "renderer/manager_bundle.h"
#include "renderer/point_cloud/point_cloud.h"
#include "renderer/scene.h"
#include "renderer/point_cloud/lidar/lidar_scan.h"
#include "renderer/point_cloud/lidar/lidar_video.h"
#include "renderer/point_cloud/gicp/gicp_pass.h"
#include "renderer/point_cloud/gicp/voxel_point_map.h"
#include "renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "renderer/point_cloud/gicp/voxel_map_point_reseter.h"

#include <vector>


float srgb_to_linear(float c) {
    if (c <= 0.04045f) {
        return c / 12.92f;
    }

    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

VkClearValue make_clear_color_srgb(glm::vec4 color) {
    VkClearValue clear_color{};

    clear_color.color.float32[0] = srgb_to_linear(color.r);
    clear_color.color.float32[1] = srgb_to_linear(color.g);
    clear_color.color.float32[2] = srgb_to_linear(color.b);
    clear_color.color.float32[3] = color.a;

    return clear_color;
}

VkClearValue clear_color = make_clear_color_srgb({0.05f, 0.05f, 0.05f, 1.0f});

struct SimpleUniform {
    glm::vec4 color;
};

struct SimpleStorage {
    glm::vec4 color1;
    glm::vec4 color2;
};

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Vulkan engine");

    QueueRequest queue_request;
    queue_request.graphics_count = 1;
    queue_request.present_count = 1;
    queue_request.compute_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);

    Camera camera;
    FPSCameraController camera_controller(camera);
    camera_controller.speed = 20;

    FrameResources frame_resources(engine.physical_device(), engine.device(), engine.num_frames_in_flight());
    
    VulkanResourceLoader resource_loader(engine, 1024 * 1024 * 100); // 1 Мб

    ShaderManager shader_manager(engine.device());
    TextureManager texture_manager(engine, resource_loader);
    MaterialManager material_manager(engine, shader_manager, frame_resources);
    ComputePassManager compute_pass_manager(engine.device(), shader_manager);
    MaterialInstanceManager material_instance_manager(engine, material_manager, texture_manager);
    MeshManager mesh_manager(engine, resource_loader);
    ManagerBundle manager_bundle(engine, shader_manager, texture_manager, material_manager, material_instance_manager, mesh_manager);

    GICPPass gicp_pass(engine, compute_pass_manager);
    VoxelMapPointInserter voxel_map_inserter(engine, compute_pass_manager);
    VoxelMapPointReseter voxel_map_reseter(engine, compute_pass_manager);

    

    // Compute pass
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // VulkanBuffer test_uniform = VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(SimpleUniform));
    // VulkanBuffer test_ssbo = VulkanBuffer::create_storage_buffer(engine, sizeof(SimpleStorage));

    // ComputePassInstance test_instance(compute_pass_manager.descriptor_pool(), compute_pass_manager.test_compute_pass);
    // test_instance.set_uniform_buffer(0, test_uniform);
    // test_instance.set_storage_buffer(1, test_ssbo);

    // VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    // VulkanFence compute_fence(engine.device());
    // {
    //     auto compute_scope = compute_command_buffer.begin_scope();
        
    //     test_instance.bind(compute_command_buffer);
    //     compute_command_buffer.dispatch(1, 1, 1);
    // }

    // engine.compute_submit(compute_command_buffer, &compute_fence);
    // compute_fence.wait();
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    Renderer renderer(engine, frame_resources);

    RenderObject unlit_cube(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube2(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube3(mesh_manager.cube, material_instance_manager.rock_blinn_phong);
    RenderObject unlit_cube4(mesh_manager.cube, material_instance_manager.unlit);

    std::vector<PointInstance> points;

    for (int x = 0; x < 100; x++)
        for (int z = 0; z < 100; z++) {
            static PointInstance point;

            point.pos.x = x * 0.01;
            point.pos.z = z * 0.01;
            point.color = glm::vec4(x / 100.0f, 0, z / 100.0f, 1);

            points.push_back(point);
        }
    
    std::vector<PointInstance> source_points;
    std::vector<glm::vec4> source_normals;

    std::vector<PointInstance> target_points;
    std::vector<glm::vec4> target_normals;

    for (int x = 0; x < 100; x++)
        for (int z = 0; z < 100; z++) {
            static PointInstance point;

            point.pos.x = x;
            point.pos.z = z;
            point.color = glm::vec4(0, 0, 1, 1);

            points.push_back(point);
        }
    
    PointCloud point_cloud(manager_bundle, points);
    // LidarScan lidar_scan(manager_bundle, path_utils::executable_dir() / "assets" / "lidar_scans" / "frame_000000.bin");
    LidarVideo lidar_video(manager_bundle, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 0, 5);

    PointCloud& target_point_cloud = lidar_video.get_scan(0).point_cloud();
    PointCloud& source_point_cloud = lidar_video.get_scan(4).point_cloud();

    VulkanBuffer target_normal_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(GICPReductor::GICPPartial) * target_point_cloud.instance_count()));
    VulkanBuffer source_normal_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(GICPReductor::GICPPartial) * source_point_cloud.instance_count()));

    VoxelPointMap voxel_point_map(engine, 1500000, 1500000);

    voxel_map_reseter.reset(voxel_point_map);
    voxel_map_inserter.insert(voxel_point_map, target_point_cloud, target_normal_buffer);

    

    PointCloud voxel_map_point_cloud(manager_bundle, voxel_point_map.map_point_buffer, voxel_point_map.m_map_point_count);

    // voxel_map_point_cloud.instance_data.external_buffer = &voxel_point_map.map_point_buffer;
    // voxel_map_point_cloud.instance_data.external_buffer = &source_point_cloud.instance_data.buffer();


    unlit_cube.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube2.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube3.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube4.set_material_data<UnlitMaterialData>({glm::vec4(0, 1, 1, 1)});

    material_instance_manager.unlit.material_buffer.sync();
    material_instance_manager.rock_blinn_phong.material_buffer.sync();
    material_instance_manager.dirt_blinn_phong.material_buffer.sync();

    unlit_cube.transform.position.x = 0;
    unlit_cube2.transform.position.x = 2;
    unlit_cube3.transform.position.x = 4;
    unlit_cube4.transform.position.x = 6;

    unlit_cube.add_child(unlit_cube2);
    unlit_cube2.add_child(unlit_cube3);
    unlit_cube3.add_child(unlit_cube4);
    unlit_cube4.add_child(point_cloud);

    Scene scene;

    // scene.add(unlit_cube);
    // scene.add(lidar_video);
    scene.add(voxel_map_point_cloud);
    scene.add(source_point_cloud);
    
    // lidar_video.set_looped(true);
    // scene.add(lidar_scan);

    bool g_pressed = false;
    
    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();
    float timer = 0;
    while (!engine.window().should_close()) {
        engine.window().poll_events();

        float current_frame_time = (float)glfwGetTime() - start_time;
        float delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        timer += delta_time;
        
        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), camera);

        if (!g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_PRESS) {
            g_pressed = true;

            gicp_pass.step(voxel_point_map, source_point_cloud, source_normal_buffer);


            // std::cout << "g pressed" << std::endl;
        }

        if (g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_RELEASE) {
            g_pressed = false;

            // std::cout << "g released" << std::endl;
        }

        // unlit_cube.transform.position.x = sin(timer) * 5 ;

        glm::vec3 axis = glm::vec3(0.0f, 1.0f, 0.0f); // Y axis
        float angle = delta_time;
        glm::quat delta = glm::angleAxis(angle, glm::normalize(axis));
        unlit_cube.transform.rotation = delta * unlit_cube.transform.rotation;

        axis = glm::vec3(0.0f, 0.0f, 1.0f); // Y axis
        angle = delta_time;
        delta = glm::angleAxis(angle, glm::normalize(axis));
        unlit_cube3.transform.rotation = delta * unlit_cube3.transform.rotation;

        axis = glm::vec3(1.0f, 0.0f, 0.0f); // Y axis
        angle = sin(timer * 3.14f / 2.0f);
        delta = glm::angleAxis(angle, glm::normalize(axis));
        unlit_cube2.transform.rotation = delta * glm::quat(1.0f, 0.0f, 0.0f, 0.0f);;

        // lidar_video.move(timer);

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                // engine.swapchain_resources().swapchain, {{0.05f, 0.05f, 0.05f, 1.0f}});
                engine.swapchain_resources().swapchain, clear_color);
                // rgba(37, 150, 190)


                renderer.render(command_buffer, scene);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
