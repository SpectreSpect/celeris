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

class VulkanEngine {
public:
    _XCLASS_NAME(VulkanEngine);

    explicit VulkanEngine(
        const GlfwContext& glfw_context,
        Window& window,
        const QueueRequest& queue_request,
        std::string_view app_name = "vulkan_engine"
    );

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
    VulkanSwapchain m_swapchain;
    std::vector<VulkanImageView> m_swapchain_image_views;
    VulkanRenderPass m_render_pass;
    std::vector<VulkanFramebuffer> m_swapchain_framebuffers;
    VulkanCommandPool m_command_pool;
    std::vector<VulkanCommandBuffer> m_command_buffers;
    std::vector<VulkanFence> m_in_flight_fences;
    std::vector<VulkanSemaphore> m_image_available_semaphores;
    std::vector<VulkanSemaphore> m_render_finished_semaphores;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    size_t m_current_frame = 0;
    

private:
    void record_command_buffer(VulkanCommandBuffer& command_buffer, uint32_t image_index);
    void draw_frame();
};
