#include "vulkan_engine.h"

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
        m_swapchain(m_physical_device, m_device, m_surface, m_window),
        m_swapchain_image_views(VulkanImageView::from_swapchain(m_device, m_swapchain)),
        m_render_pass(m_device, m_swapchain),
        m_swapchain_framebuffers(
            VulkanFramebuffer::from_image_views(
                m_swapchain_image_views, 
                m_device, 
                m_render_pass, 
                m_swapchain.extent()
            )
        ),
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
        m_render_finished_semaphores(VulkanSemaphore::create_semaphores(m_device, m_swapchain.images().size())) {}

void VulkanEngine::run() {
    LOG_METHOD();

    while (!m_window.should_close()) {
        m_window.poll_events();
        draw_frame();
    }

    m_device.wait_idle();
}

void VulkanEngine::draw_frame() {
    LOG_METHOD();

    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;
    VkResult result = m_swapchain.acquire_next_image(image_index, m_image_available_semaphores[m_current_frame]);
    logger.check(result == VK_SUCCESS, "Failed to acquire next image");
    
    m_in_flight_fences[m_current_frame].reset();
    m_command_buffers[m_current_frame].reset();
    record_command_buffer(m_command_buffers[m_current_frame], image_index);

    m_device.graphics_queue().submit(
        m_image_available_semaphores[m_current_frame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        m_command_buffers[m_current_frame],
        m_render_finished_semaphores[image_index],
        m_in_flight_fences[m_current_frame]
    );

    result = m_device.present_queue().present(
        m_render_finished_semaphores[image_index],
        m_swapchain,
        image_index
    );
    logger.check(result == VK_SUCCESS, "Failed to present image");

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::record_command_buffer(VulkanCommandBuffer& command_buffer, uint32_t image_index) {
    LOG_METHOD();
    {
        auto command_buffer_scope = command_buffer.begin_scope();
        {
            auto render_pass_scope = m_render_pass.begin_scope(
                command_buffer,
                m_swapchain_framebuffers[image_index],
                m_swapchain,
                {{0.05f, 0.08f, 0.12f, 1.0f}}
            );
        }
    }
}
