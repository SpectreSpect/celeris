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

#include <vector>

struct TestPushConstants {
    glm::mat4 model;
};

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

// std::vector<SimpleVertex> vertices = {
//     {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}}, // bottom-left
//     {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}}, // bottom-right
//     {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}, // top-right
//     {{-0.5f,  0.5f}, {1.0f, 1.0f, 0.0f}}, // top-left
// };

// std::vector<uint32_t> indices = {
//     0, 1, 2,
//     2, 3, 0
// };

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

    Transform object_transform;

    VulkanShaderModule compute_shader(engine.device(), "shaders/test_compute_shader.comp.spv");
    VulkanShaderModule vert_shader_module(engine.device(), path_utils::executable_dir() / "shaders" / "triangle.vert.spv");
    VulkanShaderModule frag_shader_module(engine.device(), path_utils::executable_dir() / "shaders" / "triangle.frag.spv");

    VulkanBuffer storage_buffer = VulkanBuffer::create_storage_buffer(engine, sizeof(SimpleStorage));
    VulkanBuffer unifrom_buffer = VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(SimpleUniform));
    VulkanBuffer vertex_buffer = VulkanBuffer::create_vertex_buffer(engine, Utils::size_bytes(cube_vertices));
    VulkanBuffer index_buffer = VulkanBuffer::create_index_buffer(engine, Utils::size_bytes(cube_indices));

    MaterialSystem material_system(engine.device());
    MaterialInstance blin_phong_red_material = material_system.create_blin_phong_material(unifrom_buffer, glm::vec4(1, 0, 0, 1));

    VulkanResourceLoader resource_loader(engine, 1024 * 1024); // 1 Мб
    resource_loader.upload_vertex_buffer(cube_vertices.data(), Utils::size_bytes(cube_vertices), vertex_buffer);
    resource_loader.upload_index_buffer(cube_indices.data(), Utils::size_bytes(cube_indices), index_buffer);
    resource_loader.upload_storage_buffer(&simple_storage, sizeof(SimpleStorage), storage_buffer);
    resource_loader.submit();

    FrameResources frame_resources(engine.physical_device(), engine.device(), engine.num_frames_in_flight());

    DescriptorSetLayoutBuilder compute_dsl_builder;
    compute_dsl_builder.add_uniform_buffer(0, ShaderStages::compute);
    compute_dsl_builder.add_storage_buffer(1, ShaderStages::compute);
    DescriptorSetLayout compute_dsl(engine.device(), compute_dsl_builder);

    DescriptorSetLayoutBuilder dsl_builder;
    dsl_builder.add_uniform_buffer(0, ShaderStages::fragment);
    dsl_builder.add_storage_buffer(1, ShaderStages::fragment);
    DescriptorSetLayout dsl(engine.device(), dsl_builder);

    DescriptorPoolBuilder pool_builder;
    pool_builder.add_layout(dsl_builder);
    pool_builder.add_layout(compute_dsl_builder);
    DescriptorPool pool(engine.device(), pool_builder);

    DescriptorSet compute_descriptor_set = pool.allocate_set(compute_dsl);
    DescriptorSet descriptor_set = pool.allocate_set(dsl);


    PipelineLayoutBuilder compute_pipeline_layout_builder = VulkanPipelineLayout::create_builder();
    compute_pipeline_layout_builder.set_device(engine.device());
    compute_pipeline_layout_builder.add_push_constants<TestPushConstants>();
    compute_pipeline_layout_builder.add_descriptor_set_layout(compute_dsl);
    VulkanPipelineLayout compute_pipeline_layout(compute_pipeline_layout_builder);

    ComputePipeline compute_pipeline(engine.device(), compute_pipeline_layout, compute_shader);

    // Graphics pipeline

    PipelineLayoutBuilder pipeline_layout_builder = VulkanPipelineLayout::create_builder();
    pipeline_layout_builder.set_device(engine.device());
    pipeline_layout_builder.add_push_constants<TestPushConstants>();
    // pipeline_layout_builder.add_descriptor_set_layout(dsl);
    pipeline_layout_builder.add_descriptor_set_layout(material_system.m_blin_phong_layout.descriptor_set_layout());
    pipeline_layout_builder.add_descriptor_set_layout(frame_resources.descriptor_layout());

    VulkanPipelineLayout pipeline_layout(pipeline_layout_builder);

    GraphicsPipelineBuilder pipeline_builder = GraphicsPipeline::create_builder();
    pipeline_builder.set_graphic_objects(engine.device(), pipeline_layout, engine.swapchain_resources().render_pass);

    VertexLayoutBuilder vertex_layout;
    vertex_layout.add_binding(0, sizeof(SimpleVertex3D));
    vertex_layout.add_attribute(0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(SimpleVertex, pos));
    // vertex_layout.add_attribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SimpleVertex, color));

    pipeline_builder.set_vertex_layout(vertex_layout);
    pipeline_builder.add_vert_shader_stage(vert_shader_module);
    pipeline_builder.add_frag_shader_stage(frag_shader_module);
    GraphicsPipeline pipeline = GraphicsPipeline(pipeline_builder);

    VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    VulkanFence compute_fence(engine.device());
    {
        auto compute_scope = compute_command_buffer.begin_scope();

        compute_descriptor_set.write_uniform_buffer(0, unifrom_buffer);
        compute_descriptor_set.write_storage_buffer(1, storage_buffer);
        
        compute_pipeline.bind(compute_command_buffer);
        compute_descriptor_set.bind(compute_command_buffer, compute_pipeline, 0);

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

    descriptor_set.write_uniform_buffer(0, unifrom_buffer);
    descriptor_set.write_storage_buffer(1, storage_buffer);

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

        // SimpleUniform ubo;
        // ubo.color = glm::vec4(1.0f, 0.0f, 1.0f, 1.0f);
        // unifrom_buffer.upload(&ubo, sizeof(SimpleUniform));

        object_transform.position.x += 1.0f * delta_time;

        static TestPushConstants pc;
        pc.model = object_transform.get_model_matrix();
        // pc.offset.x += 0.001f;

        camera_controller.update(window, delta_time);
        frame_resources.update_camera(engine.current_frame(), camera);

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain, {{0.05f, 0.08f, 0.12f, 1.0f}});

                pipeline.bind(command_buffer);
                // descriptor_set.bind(command_buffer, pipeline);
                blin_phong_red_material.bind(command_buffer, pipeline, 0);
                frame_resources.bind(engine.current_frame(), command_buffer, pipeline, 1);

                pipeline.set_y_up_viewport(command_buffer, engine);
                pipeline.set_scissor(command_buffer, engine);
                vertex_buffer.bind_as_vertex_buffer(command_buffer);
                index_buffer.bind_as_index_buffer(command_buffer);
                
                pipeline_layout.push_constants(command_buffer, pc);

                command_buffer.draw_indexed(cube_indices.size());
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
