#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/vulkan_shader_module.h"
#include "vulkan_self/vulkan_pipeline_layout.h"
#include "vulkan_self/vulkan_pipeline.h"
#include "vulkan_self/vulkan_buffer.h"

#include <vector>

struct TestPushConstants {
    glm::vec2 offset;
    float scale;
};

struct SimpleVertex {
    glm::vec2 pos;
    glm::vec3 color;
};

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Vulkan engine");
    
    QueueRequest queue_request;
    queue_request.graphics_count = 1;
    queue_request.present_count = 1;

    VulkanEngine engine(glfw_context, window, queue_request);

    PipelineLayoutBuilder pipeline_layout_builder = VulkanPipelineLayout::create_builder();
    pipeline_layout_builder.set_device(engine.device());
    pipeline_layout_builder.add_push_constants<TestPushConstants>();
    VulkanPipelineLayout pipeline_layout(pipeline_layout_builder);


    VulkanShaderModule vert_shader_module(engine.device(), "shaders/triangle.vert.spv");
    VulkanShaderModule frag_shader_module(engine.device(), "shaders/triangle.frag.spv");

    PipelineBuilder pipeline_builder = VulkanPipeline::create_builder();
    pipeline_builder.set_graphic_objects(engine.device(), pipeline_layout, engine.swapchain_resources().render_pass);

    VertexLayoutBuilder vertex_layout;
    vertex_layout.add_binding(0, sizeof(SimpleVertex));
    vertex_layout.add_attribute(0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(SimpleVertex, pos));
    vertex_layout.add_attribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(SimpleVertex, color));

    pipeline_builder.set_vertex_layout(vertex_layout);
    pipeline_builder.add_vert_shader_stage(vert_shader_module);
    pipeline_builder.add_frag_shader_stage(frag_shader_module);
    VulkanPipeline pipeline = VulkanPipeline(pipeline_builder);

    std::vector<SimpleVertex> vertices = {
        {glm::vec2{-0.5f, 0.5f}, glm::vec3{1.0f, 0.0f, 0.0f}},
        {glm::vec2{0.5f, 0.5f}, glm::vec3{0.0f, 0.0f, 1.0f}},
        {glm::vec2{0.5f, -0.5f}, glm::vec3{0.0f, 1.0f, 0.0f}},

        {glm::vec2{-0.5f, 0.5f}, glm::vec3{1.0f, 0.0f, 0.0f}},
        {glm::vec2{-0.5f, -0.5f}, glm::vec3{1.0f, 1.0f, 1.0f}},
        {glm::vec2{0.5f, -0.5f}, glm::vec3{0.0f, 1.0f, 0.0f}}
    };

    VulkanBuffer staging_buffer(
        engine.physical_device(),
        engine.device(),
        vertices.size() * sizeof(SimpleVertex),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );

    staging_buffer.upload(vertices);

    VulkanBuffer vertex_buffer(
        engine.physical_device(), 
        engine.device(),
        vertices.size() * sizeof(SimpleVertex),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VulkanCommandBuffer loading_command_buffer(engine.device(), engine.graphics_command_pool());
    VulkanFence loading_fence(engine.device());
    {
        auto loading_scope = loading_command_buffer.begin_scope();
        staging_buffer.copy_to(loading_command_buffer, vertex_buffer, vertices.size() * sizeof(SimpleVertex));
        vertex_buffer.transfer_write_to_vertex_read_barrier(loading_command_buffer);
    }

    engine.device().graphics_queue().submit(
        nullptr,
        0,
        loading_command_buffer,
        nullptr,
        &loading_fence
    );
    loading_fence.wait();

    while (!engine.window().should_close()) {
        engine.window().poll_events();
        
        uint32_t image_index = 0;
        if (!engine.aquire_free_resources(image_index)) continue;
        VulkanCommandBuffer& command_buffer = engine.get_active_command_buffer();

        // Запись команд
        {auto command_buffer_scope = command_buffer.begin_scope();
            {auto render_pass_scope = engine.swapchain_resources().render_pass.begin_scope(
                command_buffer,
                engine.swapchain_resources().framebuffers[image_index],
                engine.swapchain_resources().swapchain,
                {{0.05f, 0.08f, 0.12f, 1.0f}}
            );

                pipeline.bind(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

                VulkanPipeline::set_y_up_viewport(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                VulkanPipeline::set_scissor(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                vertex_buffer.bind_as_vertex_buffer(command_buffer);

                static TestPushConstants pc{
                    .offset = {0.0f, 0.0f},
                    .scale = 1.0f
                };

                pc.offset.x += 0.0001f;
                
                pipeline_layout.push_constants(command_buffer, pc);
                
                vkCmdDraw(
                    command_buffer.handle(),
                    6,
                    1,
                    0,
                    0
                );
            }
        }

        engine.submit_graphic_commands(image_index);
        engine.present(image_index);
    }

    engine.device().wait_idle();
}
