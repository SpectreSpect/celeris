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
#include "vulkan_shader_module.h"
#include "vulkan_buffer.h"
#include "vulkan_pipeline_layout.h"
#include "vulkan_pipeline.h"
#include "utils.h"

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

struct TestPushConstants {
    glm::vec2 offset;
    float scale;
};

struct SimpleVertex {
    glm::vec2 pos;
    glm::vec3 color;
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
    VulkanCommandPool m_command_pool;
    std::vector<VulkanCommandBuffer> m_command_buffers;
    std::vector<VulkanFence> m_in_flight_fences;
    std::vector<VulkanSemaphore> m_image_available_semaphores;

    static constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;
    size_t m_current_frame = 0;


private:
    void recreate_swapchain();
};
