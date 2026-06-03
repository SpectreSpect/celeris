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
#include "vulkan_self/material/material_instance.h"
#include "renderer/shader_manager.h"
#include "renderer/material_manager.h"
#include "vulkan_self/compute/compute_pass_builder.h"
#include "renderer/compute_pass_manager.h"
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
#include "imgui_layer.h"
#include "renderer/lighting_system/lighting_system.h"

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

VkClearValue clear_color = {0.05f, 0.05f, 0.05f, 1.0f};

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
    queue_request.graphics_count = 2;
    queue_request.present_count = 1;
    queue_request.compute_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);
    
    UI ui(window, engine);
    Camera camera;
    FPSCameraController camera_controller(camera);
    camera_controller.speed = 20;

    VulkanResourceLoader resource_loader(engine, 1024 * 1024 * 100); // 1 Мб

    ShaderManager shader_manager(engine.device());
    TextureManager texture_manager(engine, resource_loader);
    ComputePassManager compute_pass_manager(engine.device(), shader_manager);

    LightingSystem lighting_system(engine, compute_pass_manager);
    FrameResources frame_resources(engine, lighting_system, engine.num_frames_in_flight());

    lighting_system.set_frame_resources(frame_resources);

    MaterialManager material_manager(engine, shader_manager, frame_resources);
    MaterialInstanceManager material_instance_manager(engine, material_manager, texture_manager);
    MeshManager mesh_manager(engine, resource_loader);
    ManagerBundle manager_bundle(engine, shader_manager, texture_manager, material_manager, material_instance_manager, mesh_manager);

    GICPPass gicp_pass(engine, compute_pass_manager);
    VoxelMapPointInserter voxel_map_inserter(engine, compute_pass_manager);
    VoxelMapPointReseter voxel_map_reseter(engine, compute_pass_manager);

    Renderer renderer(engine, frame_resources);

    RenderObject unlit_cube(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube2(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube3(mesh_manager.cube, material_instance_manager.rock_blinn_phong);
    RenderObject unlit_cube4(mesh_manager.cube, material_instance_manager.unlit);

    // LightSource light_source{};
    // light_source.color = glm::vec4(1, 1, 1, 1);
    // light_source.position = glm::vec4(1, 1, 1, 5);

    LightSource light_source0 = {glm::vec4(1, 1, 1, 5), glm::vec4(0, 1, 0, 1)};
    LightSource light_source1 = {glm::vec4(2, 1, 0, 5), glm::vec4(1, 0, 0, 1)};
    

    // lighting_system.set_light_source(0, {glm::vec4(1, 1, 1, 5), glm::vec4(0, 1, 0, 1)});
    // lighting_system.set_light_source(1, {glm::vec4(2, 1, 0, 5), glm::vec4(1, 0, 0, 1)});
    // lighting_system.set_light_source(2, {glm::vec4(0, 1, 2, 5), glm::vec4(0, 1, 0, 1)});
    // lighting_system.set_light_source(3, {glm::vec4(-2, 1, 0, 5), glm::vec4(0, 0, 1, 1)});
    
    // LidarVideo lidar_video(manager_bundle, "/home/spectre/TEMP_lidar_output_mesh/recording/index.csv", 0, 10);

    // // Important: save original first frame pose before overwriting it.
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

    // VoxelPointMap voxel_point_map(engine, 1500000, 1500000);
    // voxel_map_reseter.reset(voxel_point_map);

    // // voxel_map_inserter.insert(voxel_point_map, target_point_cloud, target_normal_buffer);
    // voxel_map_inserter.insert(voxel_point_map, lidar_video.get_scan(0).point_cloud(), lidar_video.get_scan(0).normal_buffer());

    // PointCloud voxel_map_point_cloud(manager_bundle, voxel_point_map.map_point_buffer, voxel_point_map.m_map_point_count);

    unlit_cube.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube2.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube3.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube4.set_material_data<UnlitMaterialData>({glm::vec4(0, 1, 1, 1)});

    material_instance_manager.unlit.material_buffer.sync();
    material_instance_manager.rock_blinn_phong.material_buffer.sync();
    material_instance_manager.dirt_blinn_phong.material_buffer.sync();

    unlit_cube.transform.position = glm::vec4(0, 0, 0, 1);

    unlit_cube.transform.scale = glm::vec3(50, 1, 50);

    // unlit_cube.transform.position.x = 0;
    unlit_cube2.transform.position.x = 2;
    unlit_cube3.transform.position.x = 4;
    unlit_cube4.transform.position.x = 6;

    // unlit_cube.add_child(unlit_cube2);
    // unlit_cube2.add_child(unlit_cube3);
    // unlit_cube3.add_child(unlit_cube4);
    // unlit_cube4.add_child(point_cloud);

    Scene scene;

    // scene.add(voxel_map_point_cloud);

    scene.add(unlit_cube);

    bool g_pressed = false;
    bool n_pressed = false;

    int step = 0;
    
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

        light_source0.position.x = cos(timer) * 10;
        light_source1.position.x = sin(timer) * 10;

        lighting_system.set_light_source(0, light_source0);
        lighting_system.set_light_source(1, light_source1);

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), window, camera);
        lighting_system.update(engine.current_frame(), window, camera);

        // if (!g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_PRESS) {
        //     g_pressed = true;

        //     uint32_t current_frame_id = lidar_video.current_frame_id();

        //     if (current_frame_id > 0) {
        //         LidarScan& current_scan = lidar_video.get_scan(current_frame_id);
        //         gicp_pass.step(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
        //     }

        //     step++;
        // }

        // if (g_pressed && glfwGetKey(window.handle(), GLFW_KEY_G) == GLFW_RELEASE) {
        //     g_pressed = false;
        // }

        // if (!n_pressed && glfwGetKey(window.handle(), GLFW_KEY_N) == GLFW_PRESS) {
        //     n_pressed = true;
            
        //     uint32_t current_frame_id = lidar_video.current_frame_id();

        //     if (current_frame_id > 0) {
        //         LidarScan& current_scan = lidar_video.get_scan(current_frame_id);
        //         LidarScan& previous_scan = lidar_video.get_scan(current_frame_id - 1);

        //         PointCloud& current_point_cloud = current_scan.point_cloud();
        //         PointCloud& previous_point_cloud = previous_scan.point_cloud();

        //         current_point_cloud.transform.position = previous_point_cloud.transform.position + current_point_cloud.transform.position;

        //         current_point_cloud.transform.rotation = glm::normalize(current_point_cloud.transform.rotation * previous_point_cloud.transform.rotation);

        //         gicp_pass.fit(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer(), 10);

        //         voxel_map_inserter.insert(voxel_point_map, current_scan.point_cloud(), current_scan.normal_buffer());
        //         voxel_map_point_cloud.set_instance_view(voxel_point_map.get_map_instance_view());
        //     }
            
        //     lidar_video.next_frame();
        // }

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

                ui.begin_frame();
                ui.update_mouse_mode(window);

                ImGui::Begin("Debug");

                if (ImGui::Button("Previous frame")) {
                    
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
