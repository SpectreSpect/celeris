#include "vulkan_engine.h"

#include "utils.h"
#include "window.h"
#include "glfw_context.h"

SwapchainResources::SwapchainResources(
    const VulkanPhysicalDevice& physical_device,
    const VulkanDevice& device,
    const VulkanSurface& surface,
    Window& window)
    :   swapchain(physical_device, device, surface, window),
        image_views(VulkanImageView::from_swapchain(device, swapchain)),
        depth_format(VK_FORMAT_D32_SFLOAT),
        depth_images(
            VulkanImage::create_images(
                swapchain.images().size(),
                physical_device,
                device,
                swapchain.extent(),
                depth_format,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_TILING_OPTIMAL
            )
        ),
        depth_image_views(
            VulkanImageView::create_image_views(
                depth_images,
                device,
                VK_IMAGE_ASPECT_DEPTH_BIT)
        ),
        render_pass(device, swapchain, depth_format),
        framebuffers(
            VulkanFramebuffer::from_image_views(
                image_views,
                depth_image_views,
                device,
                render_pass,
                swapchain.extent()
            )
        ),
        render_finished_semaphores(
            VulkanSemaphore::create_semaphores(
                device,
                swapchain.images().size()
            )
        ) {}

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
        m_compute_command_pool(m_device, m_device.compute_queue()),
        m_upload_command_pool(m_device, m_device.graphics_queue()), // В теории здесь нужна transfer очередь #TODO
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

VulkanCommandPool& VulkanEngine::graphics_command_pool() noexcept {
    return m_graphics_command_pool;
}

const VulkanCommandPool& VulkanEngine::graphics_command_pool() const noexcept {
    return m_graphics_command_pool;
}


VulkanCommandPool& VulkanEngine::compute_command_pool() noexcept {
    return m_compute_command_pool;
}

const VulkanCommandPool& VulkanEngine::compute_command_pool() const noexcept {
    return m_compute_command_pool;
}

VulkanCommandPool& VulkanEngine::upload_command_pool() noexcept {
    return m_upload_command_pool;
}

const VulkanCommandPool& VulkanEngine::upload_command_pool() const noexcept {
    return m_upload_command_pool;
}

const VulkanQueue& VulkanEngine::graphics_queue(uint32_t index) const {
    return device().graphics_queue();
}

VulkanQueue& VulkanEngine::graphics_queue(uint32_t index) {
    return device().graphics_queue();
}

const VulkanQueue& VulkanEngine::present_queue(uint32_t index) const {
    return device().present_queue();
}

VulkanQueue& VulkanEngine::present_queue(uint32_t index) {
    return device().present_queue();
}

const VulkanQueue& VulkanEngine::compute_queue(uint32_t index) const {
    return device().compute_queue();
}

VulkanQueue& VulkanEngine::compute_queue(uint32_t index) {
    return device().compute_queue();
}

const VulkanQueue& VulkanEngine::transfer_queue(uint32_t index) const {
    return device().transfer_queue();
}

VulkanQueue& VulkanEngine::transfer_queue(uint32_t index) {
    return device().transfer_queue();
}

void VulkanEngine::compute_submit(
    VulkanSemaphore* wait_semaphore,
    VkPipelineStageFlags wait_stage,
    VulkanCommandBuffer& command_buffer,
    VulkanSemaphore* signal_semaphore,
    VulkanFence* fence,
    uint32_t queue_index)
{
    LOG_METHOD();
    compute_queue(queue_index).submit(
        wait_semaphore,
        wait_stage,
        command_buffer,
        signal_semaphore,
        fence
    );
}

void VulkanEngine::compute_submit(
    VulkanCommandBuffer& command_buffer,
    VulkanFence* fence,
    uint32_t queue_index)
{
    LOG_METHOD();
    compute_queue(queue_index).submit(command_buffer, fence);
}

size_t VulkanEngine::current_frame() const noexcept {
    return m_current_frame;
}

uint32_t VulkanEngine::num_frames_in_flight() const noexcept {
    return MAX_FRAMES_IN_FLIGHT;
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
