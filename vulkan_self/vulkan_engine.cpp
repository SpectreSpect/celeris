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
        m_graphics_command_pool(m_device, m_device.graphics_queue()),
        m_frame_command_buffers(
            VulkanCommandBuffer::create_command_buffers(
                m_device, 
                m_graphics_command_pool, 
                static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT)
            )
        ),
        m_in_flight_fences(VulkanFence::create_fences(m_device, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT))),
        m_image_available_semaphores(VulkanSemaphore::create_semaphores(m_device, MAX_FRAMES_IN_FLIGHT)) {}

VulkanEngine::~VulkanEngine() {

}

Window& VulkanEngine::window() noexcept {
    return m_window;
}

const Window& VulkanEngine::window() const noexcept {
    return m_window;
}

VulkanDevice& VulkanEngine::device() noexcept {
    return m_device;
}

const VulkanDevice& VulkanEngine::device() const noexcept {
    return m_device;
}

VulkanPhysicalDevice& VulkanEngine::physical_device() noexcept {
    return m_physical_device;
}

const VulkanPhysicalDevice& VulkanEngine::physical_device() const noexcept {
    return m_physical_device;
}

SwapchainResources& VulkanEngine::swapchain_resources() {
    LOG_METHOD();
    logger.check(m_swapchain_resources.has_value(), "Swapchain resources are not initialized");
    return *m_swapchain_resources;
}

const SwapchainResources& VulkanEngine::swapchain_resources() const {
    LOG_METHOD();
    logger.check(m_swapchain_resources.has_value(), "Swapchain resources are not initialized");
    return *m_swapchain_resources;
}

VulkanCommandPool& VulkanEngine::graphics_command_pool() {
    return m_graphics_command_pool;
}

const VulkanCommandPool& VulkanEngine::graphics_command_pool() const {
    return m_graphics_command_pool;
}

bool VulkanEngine::aquire_free_resources(uint32_t& free_swapchain_image_index) {
    LOG_METHOD();

    logger.check(m_current_frame < MAX_FRAMES_IN_FLIGHT, "The frame index is out of array bounds");

    m_in_flight_fences[m_current_frame].wait();

    VkResult result = m_swapchain_resources->swapchain.acquire_next_image(free_swapchain_image_index, m_image_available_semaphores[m_current_frame]);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return false;
    }

    logger.check(
        result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR,
        "Failed to acquire next image"
    );
    
    m_in_flight_fences[m_current_frame].reset();
    m_frame_command_buffers[m_current_frame].reset();

    return true;
}

VulkanCommandBuffer& VulkanEngine::get_active_command_buffer() {
    LOG_METHOD();

    logger.check(m_current_frame < MAX_FRAMES_IN_FLIGHT, "The frame index is out of array bounds");

    return m_frame_command_buffers[m_current_frame];
}

void VulkanEngine::submit_graphic_commands(uint32_t current_swapchain_image_index) {
    LOG_METHOD();

    m_device.graphics_queue().submit(
        &m_image_available_semaphores[m_current_frame],
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        m_frame_command_buffers[m_current_frame],
        &m_swapchain_resources->render_finished_semaphores[current_swapchain_image_index],
        &m_in_flight_fences[m_current_frame]
    );
}

void VulkanEngine::present(uint32_t current_swapchain_image_index) {
    LOG_METHOD();

    VkResult result = m_device.present_queue().present(
        m_swapchain_resources->render_finished_semaphores[current_swapchain_image_index],
        m_swapchain_resources->swapchain,
        current_swapchain_image_index
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
