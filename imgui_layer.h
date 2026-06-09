#pragma once

#include <cstdint>
#include <imgui.h>
#include <vulkan/vulkan.h>

#include "vulkan_self/descriptor_set/descriptor_pool.h"
#include "vulkan_self/logger/logger_header.h"

struct GLFWwindow;
class VulkanEngine;
class Window;
class VulkanCommandBuffer;


class UI {
public:
    _XCLASS_NAME(UI);

    struct VulkanInitInfo {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physical_device = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queue_family = 0;
        VkQueue queue = VK_NULL_HANDLE;

        VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
        VkRenderPass render_pass = VK_NULL_HANDLE;

        uint32_t min_image_count = 2;
        uint32_t image_count = 2;

        VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
        uint32_t subpass = 0;

        const VkAllocationCallbacks* allocator = nullptr;
        void (*check_vk_result_fn)(VkResult err) = nullptr;
    };

    explicit UI(Window& window, VulkanEngine& engine, const VulkanInitInfo& info);
    explicit UI(Window& window, VulkanEngine& engine);

    ~UI();

    void begin_frame();
    void update_mouse_mode(Window& window);
    void end_frame(VulkanCommandBuffer& command_buffer);

private:
    DescriptorPool m_descriptor_pool;

private:
    void destory() noexcept;
    DescriptorPool create_descriptor_pool(VulkanEngine& engine) const;
    VulkanInitInfo get_default_vulkan_init_info(VulkanEngine& engine, DescriptorPool& descriptor_pool);    
    static void check_vk_result(VkResult err);
    void init_imgui(Window& window, VulkanEngine& engine, const VulkanInitInfo& info);
};

// namespace ui {
//     struct VulkanInitInfo {
//         VkInstance instance = VK_NULL_HANDLE;
//         VkPhysicalDevice physical_device = VK_NULL_HANDLE;
//         VkDevice device = VK_NULL_HANDLE;
//         uint32_t queue_family = 0;
//         VkQueue queue = VK_NULL_HANDLE;

//         VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
//         VkRenderPass render_pass = VK_NULL_HANDLE;

//         uint32_t min_image_count = 2;
//         uint32_t image_count = 2;

//         VkSampleCountFlagBits msaa_samples = VK_SAMPLE_COUNT_1_BIT;
//         VkPipelineCache pipeline_cache = VK_NULL_HANDLE;
//         uint32_t subpass = 0;

//         const VkAllocationCallbacks* allocator = nullptr;
//         void (*check_vk_result_fn)(VkResult err) = nullptr;
//     };

//     void init(GLFWwindow* window, const VulkanInitInfo& info);
//     void init(Window& window, VulkanEngine& engine, DescriptorPool& descriptor_pool);
//     void check_vk_result(VkResult err);
//     void begin_frame();
//     void update_mouse_mode(Window& window);
//     void end_frame(VkCommandBuffer command_buffer);
//     void end_frame(VulkanCommandBuffer& command_buffer);
//     void set_min_image_count(uint32_t min_image_count);
//     void shutdown();
// }