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
        m_in_flight_fences(VulkanFence::create_fences(m_device, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT))) {}

VulkanEngine::~VulkanEngine() {
    destroy();
}

void VulkanEngine::destroy() {
    LOG_METHOD();

    if (m_device.handle()) {
        m_device.wait_idle();

        for (VkSemaphore semaphore : m_render_finished_semaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device.handle(), semaphore, nullptr);
            }
        }

        for (VkSemaphore semaphore : m_image_available_semaphores) {
            if (semaphore != VK_NULL_HANDLE) {
                vkDestroySemaphore(m_device.handle(), semaphore, nullptr);
            }
        }

        m_render_finished_semaphores.clear();
        m_image_available_semaphores.clear();
        m_in_flight_fences.clear();

        m_swapchain_framebuffers.clear();
        m_swapchain_image_views.clear();

        m_command_buffers.clear();
    }
}

void VulkanEngine::init() {
    LOG_METHOD();

    init_vulkan();
}

void VulkanEngine::init_vulkan() {
    LOG_METHOD();

    create_sync_objects();
}

void VulkanEngine::run() {
    LOG_METHOD();

    while (!m_window.should_close()) {
        m_window.poll_events();
        draw_frame();
    }

    m_device.wait_idle();
}

void VulkanEngine::create_sync_objects() {
    LOG_METHOD();

    m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);

    // Важно: эти semaphores ждёт vkQueuePresentKHR,
    // поэтому они должны быть по количеству swapchain images.
    m_render_finished_semaphores.resize(m_swapchain.images().size());

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult image_semaphore_result = vkCreateSemaphore(
            m_device.handle(),
            &semaphore_info,
            nullptr,
            &m_image_available_semaphores[i]
        );

        logger.check(
            image_semaphore_result == VK_SUCCESS,
            "Failed to create per-frame synchronization objects"
        );
    }

    for (size_t i = 0; i < m_render_finished_semaphores.size(); ++i) {
        VkResult render_semaphore_result = vkCreateSemaphore(
            m_device.handle(),
            &semaphore_info,
            nullptr,
            &m_render_finished_semaphores[i]
        );

        logger.check(
            render_semaphore_result == VK_SUCCESS,
            "Failed to create per-swapchain-image render finished semaphore"
        );
    }
}

void VulkanEngine::draw_frame() {
    LOG_METHOD();

    m_in_flight_fences[m_current_frame].wait();

    uint32_t image_index = 0;

    VkResult acquire_result = vkAcquireNextImageKHR(
        m_device.handle(),
        m_swapchain.handle(),
        UINT64_MAX,
        m_image_available_semaphores[m_current_frame],
        VK_NULL_HANDLE,
        &image_index
    );

    logger.check(
        acquire_result == VK_SUCCESS,
        "Failed to acquire swapchain image"
    );

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

    m_device.present_queue().present(
        m_render_finished_semaphores[image_index],
        m_swapchain,
        image_index
    );

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::record_command_buffer(VulkanCommandBuffer& command_buffer, uint32_t image_index) {
    LOG_METHOD();

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    {
        auto render_pass_commands = command_buffer.begin_scope();

        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = m_render_pass.handle();
        render_pass_info.framebuffer = m_swapchain_framebuffers[image_index].handle();

        render_pass_info.renderArea.offset = {0, 0};
        render_pass_info.renderArea.extent = m_swapchain.extent();

        VkClearValue clear_color{};
        clear_color.color = {{0.05f, 0.08f, 0.12f, 1.0f}};

        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_color;

        vkCmdBeginRenderPass(
            command_buffer.handle(),
            &render_pass_info,
            VK_SUBPASS_CONTENTS_INLINE
        );

        vkCmdEndRenderPass(command_buffer.handle());
    }
}
