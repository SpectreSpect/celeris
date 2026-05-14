#include "vulkan_engine.h"

#include <fstream>
#include <array>

VulkanEngine::VulkanEngine(
    const GlfwContext& glfw_context,
    Window& window,
    const QueueRequest& queue_request,
    std::string_view app_name)
    :   m_window(window), 
        m_instance(glfw_context, app_name),
        m_surface(m_instance, m_window),
        m_physical_device(m_instance, m_surface, queue_request),
        m_device(m_physical_device),
        m_swapchain_resources(std::in_place, m_physical_device, m_device, m_surface, m_window),
        m_command_pool(m_device, m_device.graphics_queue()),
        m_command_buffers(
            VulkanCommandBuffer::create_command_buffers(
                m_device, 
                m_command_pool, 
                static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
            )
        ),
        m_in_flight_fences(VulkanFence::create_fences(m_device, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT))),
        m_image_available_semaphores(VulkanSemaphore::create_semaphores(m_device, MAX_FRAMES_IN_FLIGHT)),
        m_pipeline_layout(create_pipeline_layout()),
        m_graphics_pipeline(create_graphics_pipeline())
    {
        
    }

VulkanEngine::~VulkanEngine() {

}

void VulkanEngine::run() {
    LOG_METHOD();

    while (!m_window.should_close()) {
        m_window.poll_events();
        draw_frame();
    }

    m_device.wait_idle();
}

void VulkanEngine::record_command_buffer(VulkanCommandBuffer& command_buffer, uint32_t image_index) {
    LOG_METHOD();
    {auto command_buffer_scope = command_buffer.begin_scope();
        
        {auto render_pass_scope = m_swapchain_resources->render_pass.begin_scope(
            command_buffer,
            m_swapchain_resources->framebuffers[image_index],
            m_swapchain_resources->swapchain,
            {{0.05f, 0.08f, 0.12f, 1.0f}}
        );

            static TestPushConstants pc{
                .offset = {0.0f, 0.0f},
                .scale = 1.0f
            };

            pc.offset.x += 0.0001f;
            
            m_pipeline_layout.push_constants(command_buffer, pc);
            
            m_graphics_pipeline.bind(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS);

            vkCmdDraw(
                command_buffer.handle(),
                3,
                1,
                0,
                0
            );
        }
    }
}

void VulkanEngine::draw_frame() {
    LOG_METHOD();

    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult result = m_swapchain_resources->swapchain.acquire_next_image(image_index, m_image_available_semaphores[m_current_frame]);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }

    logger.check(
        result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
        "Failed to acquire next image"
    );
    
    m_in_flight_fences[m_current_frame].reset();
    m_command_buffers[m_current_frame].reset();
    record_command_buffer(m_command_buffers[m_current_frame], image_index);

    m_device.graphics_queue().submit(
        m_image_available_semaphores[m_current_frame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        m_command_buffers[m_current_frame],
        m_swapchain_resources->render_finished_semaphores[image_index],
        m_in_flight_fences[m_current_frame]
    );

    result = m_device.present_queue().present(
        m_swapchain_resources->render_finished_semaphores[image_index],
        m_swapchain_resources->swapchain,
        image_index
    );
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR ||
        result == VK_SUBOPTIMAL_KHR ||
        m_window.is_window_resized)
    {
        recreate_swapchain();
    } else {
        logger.check(result == VK_SUCCESS, "Failed to present image");
    }

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::recreate_swapchain() {
    LOG_METHOD();

    m_window.is_window_resized = false;
    m_window.wait_until_framebuffer_available(); // Ждём, чтобы окно было развёрнуто

    m_device.wait_idle();

    m_swapchain_resources.reset();
    m_swapchain_resources.emplace(m_physical_device, m_device, m_surface, m_window);
}

VulkanPipelineLayout VulkanEngine::create_pipeline_layout() {
    LOG_METHOD();

    PipelineLayoutBuilder builder = VulkanPipelineLayout::create_builder();
    builder.set_device(m_device);
    builder.add_push_constants<TestPushConstants>();
    
    return VulkanPipelineLayout(builder);
}

VulkanPipeline VulkanEngine::create_graphics_pipeline() {
    LOG_METHOD();
    
    VulkanShaderModule vert_shader_module(m_device, "shaders/triangle.vert.spv");
    VulkanShaderModule frag_shader_module(m_device, "shaders/triangle.frag.spv");

    PipelineBuilder builder = VulkanPipeline::create_builder();
    builder.set_graphic_objects(m_device, m_pipeline_layout, m_swapchain_resources->render_pass);
    builder.add_vert_shader_stage(vert_shader_module);
    builder.add_frag_shader_stage(frag_shader_module);
    builder.set_viewport(m_swapchain_resources->swapchain.extent());
    builder.set_scissor(m_swapchain_resources->swapchain.extent());

    return VulkanPipeline(builder);
}
