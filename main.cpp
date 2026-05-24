#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/vulkan_shader_module.h"
#include "vulkan_self/pipeline/vulkan_pipeline_layout.h"
#include "vulkan_self/pipeline/graphics_pipeline.h"
#include "vulkan_self/vulkan_buffer.h"
#include "vulkan_self/vulkan_resource_loader.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout_builder.h"
#include "vulkan_self/descriptor_set/descriptor_set_layout.h"
#include "vulkan_self/descriptor_set/descriptor_pool_builder.h"
#include "vulkan_self/descriptor_set/descriptor_pool.h"
#include "vulkan_self/descriptor_set/descriptor_set.h"
#include "vulkan_self/image/vulkan_image.h"
#include "vulkan_self/image/cpu_image.h"
#include "vulkan_self/image/vulkan_texture_2d.h"
#include "vulkan_self/pipeline/compute_pipeline.h"
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
#include "vulkan_self/material/material_buffer.h"
#include "renderer/instanced_render_object.h"
#include "renderer/instance_batch.h"
#include "renderer/texture_manager.h"
#include "renderer/material_instance_manager.h"
#include "renderer/point_instance.h"
#include "renderer/renderer.h"
#include "renderer/mesh_manager.h"

#include <vector>

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

    FrameResources frame_resources(engine.physical_device(), engine.device(), engine.num_frames_in_flight());
    
    VulkanResourceLoader resource_loader(engine, 1024 * 1024 * 100); // 1 Мб

    ShaderManager shader_manager(engine.device());
    TextureManager texture_manager(engine, resource_loader);
    MaterialManager material_manager(engine, shader_manager, frame_resources);
    MaterialInstanceManager material_instance_manager(engine, material_manager, texture_manager);
    MeshManager mesh_manager(engine, resource_loader);

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

    InstancedRenderObject point_cloud(engine, mesh_manager.point_cloud_quad, 
                                      material_instance_manager.point_cloud, points.size(), sizeof(PointInstance));

    point_cloud.instance_data.buffer().upload(points.data(), points.size() * sizeof(PointInstance));

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

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain, {{0.05f, 0.08f, 0.12f, 1.0f}});

                renderer.render(command_buffer, unlit_cube);
                renderer.render(command_buffer, unlit_cube2);
                renderer.render(command_buffer, unlit_cube3);
                renderer.render(command_buffer, unlit_cube4);

                renderer.render(command_buffer, point_cloud);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
