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

// struct DrawElementsIndirectCommand {
//     uint32_t count;
//     uint32_t instanceCount;
//     uint32_t firstIndex;
//     int32_t  baseVertex;
//     uint32_t baseInstance;
// };

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
    ComputePassManager compute_pass_manager(engine.device(), shader_manager);
    TextureManager texture_manager(engine, resource_loader, compute_pass_manager);
    
    LightingSystem lighting_system(engine, compute_pass_manager);
    FrameResources frame_resources(engine, lighting_system, engine.num_frames_in_flight());

    lighting_system.set_frame_resources(frame_resources);

    MaterialManager material_manager(engine, shader_manager, frame_resources);
    MaterialInstanceManager material_instance_manager(engine, material_manager, texture_manager);
    MeshManager mesh_manager(engine, resource_loader);
    ManagerBundle manager_bundle(engine, shader_manager, texture_manager, material_manager, material_instance_manager, mesh_manager);

    // CubemapArray cubemaps(engine.physical_device(), engine.device(), VkExtent2D{512, 512}, VK_FORMAT_R8G8B8A8_UNORM, 16, CubemapArray::StorageImageUsage::Enabled);

    // EquirectToCubemapPass equirect_to_cubemap_pass(engine, compute_pass_manager);

    // BrdfLutPass brdf_lut_generator(engine, compute_pass_manager);
    // VulkanTexture2D brdf_lut_texture = brdf_lut_generator.generate(256, 256);

    // PrefilterPass prefilter_pass(engine, compute_pass_manager);
    // Cubemap prefilter_map = prefilter_pass.generate(*texture_manager.st_peters_square_night_4k_hdr_env_map, 32);

    // Cubemap dirt_cubemap = equirect_to_cubemap_pass.generate(texture_manager.dirt_texture, 100);

    // IrradiancePass irradiance_pass(engine, compute_pass_manager);
    // Cubemap irradiance_map = irradiance_pass.generate(*texture_manager.st_peters_square_night_4k_hdr_env_map, 32);

    // Cubemap dirt_cubemap = equirect_to_cubemap_pass.generate(texture_manager.dirt_texture, 100);


        // float voxel_size = 1.0f;
        // uint32_t chunk_size = 16u;
        // glm::ivec3(chunk_size), // chunk_size
        // glm::vec3(voxel_size), // voxel_size
        // 10'000, // count_active_chunks
        // 1'000'000, // max_quads
        // 4, // chunk_hash_table_size_factor
        // 4096, // count_evict_buckets
        // 5'000, // min_free_chunks
        // 0.2f, // tomb_fraction_to_rebuild
        // chunk_size * voxel_size * 1, // eviction_bucket_shell_thickness
        // 10, // vb_page_size_order_of_two
        // 10, // ib_page_size_order_of_two
        // 1.0, // buddy_allocator_nodes_factor

    glm::vec3 voxel_size(1.0f);
    glm::ivec3 chunk_size(16);
    VoxelGrid::VoxelGridDesc voxel_grid_desc {
        .chunk_size = chunk_size,
        .voxel_size = voxel_size,
        .count_active_chunks = 10'000,
        .max_quads = 3'000'000,
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

    VoxelGrid voxel_grid(engine.physical_device(), 
                         engine.device(), engine.compute_queue(), 
                         compute_pass_manager, material_instance_manager, 
                         voxel_grid_desc);


    GICPPass gicp_pass(engine, compute_pass_manager);
    VoxelMapPointInserter voxel_map_inserter(engine, compute_pass_manager);
    VoxelMapPointReseter voxel_map_reseter(engine, compute_pass_manager);

    Renderer renderer(engine, frame_resources);
    
    VulkanBuffer indirect_command_buffer(engine.physical_device(), 
                                         engine.device(), 
                                         sizeof(uint32_t) + sizeof(DrawElementsIndirectCommand) * 2,
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    RenderObject sphere(mesh_manager.sphere, material_instance_manager.pbr);

    
    
    // uint32_t indirect_command_count = 1;
    // DrawElementsIndirectCommand commands {
    //     .count = sphere.mesh_view().index_count(), 
    //     .instanceCount = 1,
    //     .firstIndex = 0,
    //     .baseVertex = 0,
    //     .baseInstance = 0
    // };

    // indirect_command_buffer.upload(&indirect_command_count, sizeof(uint32_t), 0);
    // indirect_command_buffer.upload(&commands, sizeof(DrawElementsIndirectCommand), sizeof(uint32_t));


    uint32_t draw_count = 2;

    VkDrawIndexedIndirectCommand commands[2] = {
        {
            .indexCount    = StaticMeshData::two_sphere_inidirect_test.sphere_index_count,
            .instanceCount = 1,
            .firstIndex    = StaticMeshData::two_sphere_inidirect_test.first_sphere_first_index,
            .vertexOffset  = StaticMeshData::two_sphere_inidirect_test.first_sphere_base_vertex,
            .firstInstance = 0
        },
        {
            .indexCount    = StaticMeshData::two_sphere_inidirect_test.sphere_index_count,
            .instanceCount = 1,
            .firstIndex    = StaticMeshData::two_sphere_inidirect_test.second_sphere_first_index,
            .vertexOffset  = StaticMeshData::two_sphere_inidirect_test.second_sphere_base_vertex,
            .firstInstance = 0
        }
    };

    indirect_command_buffer.upload(&draw_count, sizeof(uint32_t), 0);
    indirect_command_buffer.upload(&commands, sizeof(DrawElementsIndirectCommand) * 2, sizeof(uint32_t));


    IndirectRenderObject two_spheres(mesh_manager.two_sphere_indirect_test, material_instance_manager.pbr, indirect_command_buffer, 2);

    RenderObject unlit_cube(mesh_manager.cube, material_instance_manager.pbr);
    RenderObject unlit_cube2(mesh_manager.cube, material_instance_manager.dirt_blinn_phong);
    RenderObject unlit_cube3(mesh_manager.cube, material_instance_manager.rock_blinn_phong);
    RenderObject unlit_cube4(mesh_manager.cube, material_instance_manager.unlit);

    const float skybox_exposure = 1.8f;

    Skybox skybox(
        mesh_manager.skybox_cube,
        material_instance_manager.st_peters_square_night_4k_hdr,
        texture_manager,
        material_manager.pbr_mp,
        TextureManager::st_peters_square_night_4k_pbr_map_id,
        skybox_exposure
    );

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

    // unlit_cube.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube.set_material_data<PBRMaterialData>(PBRMaterialData::create(1.0f, 0.01f, skybox_exposure));
    sphere.set_material_data(PBRMaterialData::create(1.0f, 0.01f, skybox_exposure));
    two_spheres.set_material_data(PBRMaterialData::create(1.0f, 0.01f, skybox_exposure));

    unlit_cube2.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube3.set_material_data<BlinPhongMaterialData>({glm::vec4(0.1, 1, 0.5, 32.0), glm::vec4(1, 1, 1, 1)});
    unlit_cube4.set_material_data<UnlitMaterialData>({glm::vec4(0, 1, 1, 1)});

    material_instance_manager.unlit.sync();
    material_instance_manager.rock_blinn_phong.sync();
    material_instance_manager.dirt_blinn_phong.sync();

    unlit_cube.transform.position = glm::vec4(0, 0, 0, 1);

    unlit_cube.transform.scale = glm::vec3(50, 1, 50);
    sphere.transform.position = glm::vec4(0, 2, 0, 1);

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

    sphere.add_child(two_spheres);

    // scene.add(unlit_cube);
    scene.add(sphere);
    scene.add(skybox);
    

    skybox.update(scene);

    bool g_pressed = false;
    bool n_pressed = false;

    int step = 0;
    
    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();
    float timer = 0;
    uint32_t pending_skybox_environment_map_id = skybox.environment_map_id();
    bool skybox_environment_update_pending = false;

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
        
        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();

        light_source0.position.x = cos(timer) * 10;
        light_source1.position.x = sin(timer) * 10;

        lighting_system.set_light_source(0, light_source0);
        lighting_system.set_light_source(1, light_source1);

        voxel_grid.update();
        
        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), window, camera);
        lighting_system.update(engine.current_frame(), window, camera);

        sphere.transform.position.y = sin(timer) * 2;
        two_spheres.transform.position.x = cos(timer) * 10;
        

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

                // renderer.render_indirect(command_buffer, two_spheres, indirect_command_buffer, sizeof(uint32_t), 0, 2);

                ui.begin_frame();
                ui.update_mouse_mode(window);

                ImGui::Begin("Debug");

                // if (ImGui::Button("Previous frame")) {
                //     pending_skybox_environment_map_id = TextureManager::studio_kominka_02_4k_pbr_map_id;
                //     skybox_environment_update_pending = true;
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
