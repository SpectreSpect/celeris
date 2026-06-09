#pragma once

#include <array>
#include <vector>
#include <string>
#include <utility>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "glfw_context.h"
#include "logger/logger_header.h"

class VulkanInstance {
public:
    _XCLASS_NAME(VulkanInstance);

    explicit VulkanInstance(const GlfwContext&, std::string_view app_name = "VulkanEngine");
    ~VulkanInstance();
    void destroy();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    VulkanInstance(VulkanInstance&& other) noexcept;
    VulkanInstance& operator=(VulkanInstance&& other) noexcept;

    const std::string& app_name() const noexcept;
    VkInstance handle() const noexcept;
    VkDebugUtilsMessengerEXT debug_messenger() const noexcept;

#ifdef NDEBUG
    static constexpr bool enable_validation_layers = false;
#else
    static constexpr bool enable_validation_layers = true;
#endif

    static constexpr std::array validation_layers = {
        "VK_LAYER_KHRONOS_validation"
    };

private:
    void setup_debug_messenger();

    bool check_validation_layer_support() const;
    std::vector<const char*> get_required_extensions() const;

    void populate_debug_messenger_create_info(
        VkDebugUtilsMessengerCreateInfoEXT& create_info
    ) const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data
    );

    static VkResult create_debug_utils_messenger_ext(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* create_info,
        const VkAllocationCallbacks* allocator,
        VkDebugUtilsMessengerEXT* debug_messenger
    );

    static void destroy_debug_utils_messenger_ext(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debug_messenger,
        const VkAllocationCallbacks* allocator
    );

private:
    std::string m_app_name;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
};
