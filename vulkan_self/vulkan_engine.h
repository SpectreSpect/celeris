#pragma once

#include <vector>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

#include "vulkan_instance.h"
#include "vulkan_surface.h"
#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_render_pass.h"
#include "vulkan_command_pool.h"
#include "vulkan_framebuffer.h"
#include "image/vulkan_image.h"
#include "image/vulkan_image_view.h"
#include "vulkan_semaphore.h"
#include "vulkan_fence.h"
#include "vulkan_command_buffer.h"

class Window;
class GlfwContext;

struct SwapchainResources {
    VulkanSwapchain swapchain;
    std::vector<VulkanImageView> image_views;

    VkFormat depth_format;
    std::vector<VulkanImage> depth_images;
    std::vector<VulkanImageView> depth_image_views;

    VulkanRenderPass render_pass;
    std::vector<VulkanFramebuffer> framebuffers;
    std::vector<VulkanSemaphore> render_finished_semaphores;

    SwapchainResources(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        const VulkanSurface& surface,
        Window& window
    );
};

class VulkanEngine {
public:
    _XCLASS_NAME(VulkanEngine);

    explicit VulkanEngine(
        const GlfwContext& glfw_context,
        Window& window,
        const QueueRequest& queue_request,
        std::string_view app_name = "vulkan_engine"
    );

    ~VulkanEngine();

    VulkanEngine(const VulkanEngine&) = delete;
    VulkanEngine& operator=(const VulkanEngine&) = delete;

    VulkanEngine(VulkanEngine&&) = delete;
    VulkanEngine& operator=(VulkanEngine&&) = delete;

    Window& window() noexcept;
    const Window& window() const noexcept;

    VulkanDevice& device() noexcept;
    const VulkanDevice& device() const noexcept;

    VulkanPhysicalDevice& physical_device() noexcept;
    const VulkanPhysicalDevice& physical_device() const noexcept;

    SwapchainResources& swapchain_resources();
    const SwapchainResources& swapchain_resources() const;

    VulkanCommandPool& graphics_command_pool() noexcept;
    const VulkanCommandPool& graphics_command_pool() const noexcept;

    VulkanCommandPool& upload_command_pool() noexcept;
    const VulkanCommandPool& upload_command_pool() const noexcept;

    VulkanCommandPool& compute_command_pool() noexcept;
    const VulkanCommandPool& compute_command_pool() const noexcept;

    const VulkanQueue& graphics_queue(uint32_t index = 0) const;
    VulkanQueue& graphics_queue(uint32_t index = 0);

    const VulkanQueue& present_queue(uint32_t index = 0) const;
    VulkanQueue& present_queue(uint32_t index = 0);
    
    const VulkanQueue& compute_queue(uint32_t index = 0) const;
    VulkanQueue& compute_queue(uint32_t index = 0);
    
    const VulkanQueue& transfer_queue(uint32_t index = 0) const;
    VulkanQueue& transfer_queue(uint32_t index = 0);

    void compute_submit(
        VulkanSemaphore* wait_semaphore,
        VkPipelineStageFlags wait_stage,
        VulkanCommandBuffer& command_buffer,
        VulkanSemaphore* signal_semaphore,
        VulkanFence* fence,
        uint32_t queue_index = 0
    );

    void compute_submit(
        VulkanCommandBuffer& command_buffer,
        VulkanFence* fence = nullptr,
        uint32_t queue_index = 0
    );

    size_t current_frame() const noexcept;
    uint32_t num_frames_in_flight() const noexcept;

    bool aquire_free_resources(uint32_t& free_swapchain_image_index);
    VulkanCommandBuffer& get_active_command_buffer();
    void submit_graphic_commands(uint32_t current_swapchain_image_index);
    void present(uint32_t current_swapchain_image_index);

private:
    Window& m_window;
    VulkanInstance m_instance;
    VulkanSurface m_surface;
    VulkanPhysicalDevice m_physical_device;
    VulkanDevice m_device;
    std::optional<SwapchainResources> m_swapchain_resources;
    VulkanCommandPool m_graphics_command_pool;
    VulkanCommandPool m_compute_command_pool;
    VulkanCommandPool m_upload_command_pool;
    std::vector<VulkanCommandBuffer> m_frame_command_buffers;
    std::vector<VulkanFence> m_in_flight_fences;
    std::vector<VulkanSemaphore> m_image_available_semaphores;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    size_t m_current_frame = 0;

private:
    void recreate_swapchain();
};
