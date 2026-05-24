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
#include "renderer/shader_manager.h"
#include "renderer/material_manager.h"
#include "vulkan_self/compute/compute_pass_builder.h"
#include "renderer/compute_pass_manager.h"
#include "renderer/compute_pass_instance.h"

#include <vector>


class Renderer {
public:
    Renderer(VulkanEngine& engine, FrameResources& frame_resources) {
        m_engine = &engine;
        m_frame_resources = &frame_resources;
    };

    void render(VulkanCommandBuffer& command_buffer, RenderObject& render_object) {
        static TransformPushConstants pc;
        pc.model = render_object.transform.get_model_matrix();

        // blinn_phong_material_instance.bind(command_buffer);
        render_object.m_material->bind(command_buffer);
        MaterialPass& pass = render_object.m_material->m_pass;

        m_frame_resources->bind(m_engine->current_frame(), command_buffer, pass.pipeline(), 1);

        pass.pipeline().set_y_up_viewport(command_buffer, *m_engine);
        pass.pipeline().set_scissor(command_buffer, *m_engine);

        render_object.m_mesh.bind_vertex_buffer(command_buffer);
        render_object.m_mesh.bind_index_buffer(command_buffer);

        pass.pipeline_layout().push_constants(command_buffer, pc);
        
        command_buffer.draw_indexed(render_object.m_mesh.index_count());
    };

private:
    VulkanEngine* m_engine = nullptr;
    FrameResources* m_frame_resources = nullptr;
};



// struct TestPushConstants {
//     glm::mat4 model;
// };

struct SimpleVertex {
    glm::vec2 pos;
    glm::vec3 color;
};



struct SimpleUniform {
    glm::vec4 color;
};

struct SimpleStorage {
    glm::vec4 color1;
    glm::vec4 color2;
};

struct SimpleVertex3D {
    glm::vec4 position;
    glm::vec4 normal;
    glm::vec2 uv;
};

std::vector<SimpleVertex3D> cube_vertices = {
    // Front face, normal +Z
    {{-0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 0
    {{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 1
    {{ 0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 2
    {{-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // 3

    // Back face, normal -Z
    {{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}}, // 4
    {{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}}, // 5
    {{-0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}}, // 6
    {{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}}, // 7

    // Left face, normal -X
    {{-0.5f, -0.5f, -0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 8
    {{-0.5f, -0.5f,  0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 9
    {{-0.5f,  0.5f,  0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 10
    {{-0.5f,  0.5f, -0.5f, 1.0f}, {-1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 11

    // Right face, normal +X
    {{ 0.5f, -0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 12
    {{ 0.5f, -0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 13
    {{ 0.5f,  0.5f, -0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 14
    {{ 0.5f,  0.5f,  0.5f, 1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 15

    // Top face, normal +Y
    {{-0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 16
    {{-0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 17
    {{ 0.5f,  0.5f,  0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 18
    {{ 0.5f,  0.5f, -0.5f, 1.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 19

    // Bottom face, normal -Y
    {{-0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, -1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}}, // 20
    {{ 0.5f, -0.5f, -0.5f, 1.0f}, {0.0f, -1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}}, // 21
    {{ 0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, -1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}}, // 22
    {{-0.5f, -0.5f,  0.5f, 1.0f}, {0.0f, -1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}}, // 23
};

std::vector<uint32_t> cube_indices = {
    // Front face
    0, 1, 2,
    2, 3, 0,

    // Back face
    4, 5, 6,
    6, 7, 4,

    // Left face
    8, 9, 10,
    10, 11, 8,

    // Right face
    12, 13, 14,
    14, 15, 12,

    // Top face
    16, 17, 18,
    18, 19, 16,

    // Bottom face
    20, 21, 22,
    22, 23, 20
};

SimpleStorage simple_storage{glm::vec4(1, 0, 0, 1), glm::vec4(0, 0, 1, 1)};

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

    ShaderManager shader_manager(engine.device());
    MaterialManager material_manager(engine, shader_manager, frame_resources);
    ComputePassManager compute_pass_manager(engine.device(), shader_manager);

    Renderer renderer(engine, frame_resources);

    CpuImage dirt_cpu_image = CpuImage::load_rgba8_image(
        path_utils::executable_dir() / "assets" / "textures" / "minecraft_dirt" / "texture.png"
    );

    VulkanTexture2D dirt_texture(
        engine.physical_device(),
        engine.device(),
        dirt_cpu_image.extent2d()
    );

    VulkanResourceLoader resource_loader(engine, 1024 * 1024); // 1 Мб
    resource_loader.upload_sampled_texture_2d(dirt_cpu_image, dirt_texture);
    resource_loader.submit();


    VulkanBuffer test_uniform = VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(SimpleUniform));
    VulkanBuffer test_ssbo = VulkanBuffer::create_storage_buffer(engine, sizeof(SimpleStorage));

    ComputePassInstance test_instance(compute_pass_manager.descriptor_pool(), compute_pass_manager.test_compute_pass);
    test_instance.set_uniform_buffer(0, test_uniform);
    test_instance.set_storage_buffer(1, test_ssbo);

    VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    VulkanFence compute_fence(engine.device());
    {
        auto compute_scope = compute_command_buffer.begin_scope();
        
        test_instance.bind(compute_command_buffer);
        compute_command_buffer.dispatch(1, 1, 1);
    }

    engine.device().compute_queue().submit(
        nullptr,
        0,
        compute_command_buffer,
        nullptr,
        &compute_fence
    );
    compute_fence.wait();









    BlinnPhongMaterialInstance blinn_phong_material_instance(engine, material_manager.descriptor_pool(), material_manager.blin_phong_mp, dirt_texture);
    blinn_phong_material_instance.descriptor_set.write_uniform_buffer(2, test_uniform);
    blinn_phong_material_instance.descriptor_set.write_storage_buffer(3, test_ssbo);

    UnlitMaterialInstance unlit_material_instance(engine, material_manager.descriptor_pool(), material_manager.unlit_mp);

    blinn_phong_material_instance.set_color(glm::vec4(1, 0, 0, 1));
    unlit_material_instance.set_color(glm::vec4(0, 0, 1, 1));

    Mesh cube_mesh(engine, resource_loader, cube_vertices.data(), Utils::size_bytes(cube_vertices), 
                                       cube_indices.data(), Utils::size_bytes(cube_indices));

    RenderObject blinn_phong_cube(cube_mesh, blinn_phong_material_instance);
    RenderObject unlit_cube(cube_mesh, unlit_material_instance);

    unlit_cube.transform.position.x = 2;

    
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
        
        // cube.transform.position.x += 1.0f * delta_time;

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), camera);

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain, {{0.05f, 0.08f, 0.12f, 1.0f}});

                unlit_cube.transform.position.x = 3 + sin(timer);
                blinn_phong_cube.transform.position.y = cos(timer);
                
                renderer.render(command_buffer, unlit_cube);
                renderer.render(command_buffer, blinn_phong_cube);
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
