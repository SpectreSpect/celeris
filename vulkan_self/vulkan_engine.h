#pragma once

#include <set>
#include <vector>
#include <string>
#include <limits>
#include <cstdint>
#include <iostream>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "glfw_context.h"
#include "window.h"

#include "vulkan_instance.h"
#include "vulkan_surface.h"
#include "vulkan_physical_device.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_image_view.h"
#include "vulkan_render_pass.h"
#include "vulkan_framebuffer.h"
#include "vulkan_command_pool.h"
#include "vulkan_command_buffer.h"
#include "vulkan_fence.h"
#include "vulkan_semaphore.h"

struct SwapchainResources {
    VulkanSwapchain swapchain;
    std::vector<VulkanImageView> image_views;
    VulkanRenderPass render_pass;
    std::vector<VulkanFramebuffer> framebuffers;
    std::vector<VulkanSemaphore> render_finished_semaphores;

    SwapchainResources(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        const VulkanSurface& surface,
        Window& window)
        :   swapchain(physical_device, device, surface, window),
            image_views(VulkanImageView::from_swapchain(device, swapchain)),
            render_pass(device, swapchain),
            framebuffers(
                VulkanFramebuffer::from_image_views(
                    image_views,
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

    void run();

private:
    Window& m_window;
    VulkanInstance m_instance;
    VulkanSurface m_surface;
    VulkanPhysicalDevice m_physical_device;
    VulkanDevice m_device;
    std::optional<SwapchainResources> m_swapchain_resources;
    VulkanCommandPool m_command_pool;
    std::vector<VulkanCommandBuffer> m_command_buffers;
    std::vector<VulkanFence> m_in_flight_fences;
    std::vector<VulkanSemaphore> m_image_available_semaphores;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    size_t m_current_frame = 0;

    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;

private:
    void record_command_buffer(VulkanCommandBuffer& command_buffer, uint32_t image_index);
    void draw_frame();
    void recreate_swapchain();

    VkShaderModule create_shader_module(const std::vector<char>& code);
    void create_graphics_pipeline();
    void cleanup_graphics_pipeline();
};
