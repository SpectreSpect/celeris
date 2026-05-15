#include "vulkan_self/vulkan_engine.h"

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
    pipeline_builder.add_vert_shader_stage(vert_shader_module);
    pipeline_builder.add_frag_shader_stage(frag_shader_module);
    VulkanPipeline pipeline = VulkanPipeline(pipeline_builder);

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

                VulkanPipeline::set_viewport(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                VulkanPipeline::set_scissor(
                    command_buffer,
                    engine.swapchain_resources().swapchain.extent()
                );

                static TestPushConstants pc{
                    .offset = {0.0f, 0.0f},
                    .scale = 1.0f
                };

                pc.offset.x += 0.0001f;
                
                pipeline_layout.push_constants(command_buffer, pc);
                
                vkCmdDraw(
                    command_buffer.handle(),
                    3,
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
