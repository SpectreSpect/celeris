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

#include <vector>

// struct TestPushConstants {
//     glm::mat4 model;
// };

struct SimpleVertex {
    glm::vec2 pos;
    glm::vec3 color;
};

struct SimpleVertex3D {
    glm::vec4 pos;
};

struct SimpleUniform {
    glm::vec4 color;
};

struct SimpleStorage {
    glm::vec4 color1;
    glm::vec4 color2;
};

std::vector<SimpleVertex3D> cube_vertices = {
    {{-0.5f, -0.5f, -0.5f, 1.0f}}, // 0: back-bottom-left
    {{ 0.5f, -0.5f, -0.5f, 1.0f}}, // 1: back-bottom-right
    {{ 0.5f,  0.5f, -0.5f, 1.0f}}, // 2: back-top-right
    {{-0.5f,  0.5f, -0.5f, 1.0f}}, // 3: back-top-left

    {{-0.5f, -0.5f,  0.5f, 1.0f}}, // 4: front-bottom-left
    {{ 0.5f, -0.5f,  0.5f, 1.0f}}, // 5: front-bottom-right
    {{ 0.5f,  0.5f,  0.5f, 1.0f}}, // 6: front-top-right
    {{-0.5f,  0.5f,  0.5f, 1.0f}}, // 7: front-top-left
};

std::vector<uint32_t> cube_indices = {
    // Front face
    4, 5, 6,
    6, 7, 4,

    // Back face
    1, 0, 3,
    3, 2, 1,

    // Left face
    0, 4, 7,
    7, 3, 0,

    // Right face
    5, 1, 2,
    2, 6, 5,

    // Top face
    3, 7, 6,
    6, 2, 3,

    // Bottom face
    0, 1, 5,
    5, 4, 0
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

    VulkanShaderModule vert_shader_module(engine.device(), path_utils::executable_dir() / "shaders" / "triangle.vert.spv");
    VulkanShaderModule frag_shader_module(engine.device(), path_utils::executable_dir() / "shaders" / "triangle.frag.spv");

    MaterialSystem material_system(engine.device());

    VulkanResourceLoader resource_loader(engine, 1024 * 1024); // 1 Мб

    Mesh cube_mesh(engine, resource_loader, cube_vertices.data(), Utils::size_bytes(cube_vertices), 
                                       cube_indices.data(), Utils::size_bytes(cube_indices));
    RenderObject cube(cube_mesh);

    FrameResources frame_resources(engine.physical_device(), engine.device(), engine.num_frames_in_flight());

    MaterialPass blin_phong_material_pass = material_system.create_blin_phong_pass(engine, frame_resources.descriptor_layout(), vert_shader_module, frag_shader_module);

    BlinnPhongMaterialInstance blinn_phong_material_instance(engine, material_system.m_descriptor_pool, blin_phong_material_pass);
    
    blinn_phong_material_instance.set_color(glm::vec4(1, 0, 0, 1));

    float last_frame_time = 0.0f;
    float start_time = (float)glfwGetTime();

    while (!engine.window().should_close()) {
        engine.window().poll_events();

        float current_frame_time = (float)glfwGetTime() - start_time;
        float delta_time = current_frame_time - last_frame_time;
        last_frame_time = current_frame_time;
        
        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();
        
        cube.transform.position.x += 1.0f * delta_time;

        static TransformPushConstants pc;
        pc.model = cube.transform.get_model_matrix();

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), camera);

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain, {{0.05f, 0.08f, 0.12f, 1.0f}});

                blinn_phong_material_instance.bind(command_buffer);
                MaterialPass& pass = blinn_phong_material_instance.m_pass;

                frame_resources.bind(engine.current_frame(), command_buffer, pass.pipeline(), 1);

                pass.pipeline().set_y_up_viewport(command_buffer, engine);
                pass.pipeline().set_scissor(command_buffer, engine);

                cube.m_mesh.bind_vertex_buffer(command_buffer);
                cube.m_mesh.bind_index_buffer(command_buffer);

                pass.pipeline_layout().push_constants(command_buffer, pc);
                
                command_buffer.draw_indexed(cube.m_mesh.index_count());
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
