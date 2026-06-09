#include "vulkan_instance.h"

VulkanInstance::VulkanInstance(const GlfwContext&, std::string_view app_name) : m_app_name(app_name) {
    LOG_METHOD();

    logger.log() << "glfwVulkanSupported: " << clr(std::to_string(glfwVulkanSupported()), "#2c87ff") << "\n";
    logger.check(glfwVulkanSupported(), "GLFW reports Vulkan is not supported on this machine");

    if (enable_validation_layers) {
        logger.check(
            check_validation_layer_support(),
            "Validation layers requested, but not available"
        );
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = m_app_name.c_str();
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "VulkanEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = get_required_extensions();

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(validation_layers.size());
        create_info.ppEnabledLayerNames = validation_layers.data();

        populate_debug_messenger_create_info(debug_create_info);
        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
        create_info.pNext = nullptr;
    }

    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    logger.check(result == VK_SUCCESS, "Failed to create Vulkan instance");

    setup_debug_messenger();
}

VulkanInstance::~VulkanInstance() {
    destroy();
}

void VulkanInstance::destroy() {
    if (m_instance != VK_NULL_HANDLE) {
        if (m_debug_messenger != VK_NULL_HANDLE) {
            destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
            m_debug_messenger = VK_NULL_HANDLE;
        }

        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VulkanInstance::VulkanInstance(VulkanInstance&& other) noexcept
    :   m_app_name(std::move(other.m_app_name)),
        m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)),
        m_debug_messenger(std::exchange(other.m_debug_messenger, VK_NULL_HANDLE)) {}

VulkanInstance& VulkanInstance::operator=(VulkanInstance&& other) noexcept {
    if (this != &other) {
        destroy();

        m_app_name = std::move(other.m_app_name);
        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
        m_debug_messenger = std::exchange(other.m_debug_messenger, VK_NULL_HANDLE);
    }

    return *this;
}

const std::string& VulkanInstance::app_name() const noexcept {
    return m_app_name;
}

VkInstance VulkanInstance::handle() const noexcept { 
    return m_instance;
}

VkDebugUtilsMessengerEXT VulkanInstance::debug_messenger() const noexcept {
    return m_debug_messenger;
}

void VulkanInstance::setup_debug_messenger() {
    LOG_METHOD();

    if (!enable_validation_layers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    populate_debug_messenger_create_info(create_info);

    VkResult result = create_debug_utils_messenger_ext(
        m_instance,
        &create_info,
        nullptr,
        &m_debug_messenger
    );

    logger.check(result == VK_SUCCESS, "Failed to set up debug messenger");
}

bool VulkanInstance::check_validation_layer_support() const {
    LOG_METHOD();

    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* required_layer_name : validation_layers) {
        bool found = false;

        for (const auto& available_layer : available_layers) {
            if (std::string(required_layer_name) == available_layer.layerName) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanInstance::get_required_extensions() const {
    LOG_METHOD();

    logger.check(glfwVulkanSupported(), "GLFW reports Vulkan is not supported on this machine");

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    logger.check(glfw_extensions != nullptr, "glfwGetRequiredInstanceExtensions returned nullptr");
    logger.check(glfw_extension_count > 0, "GLFW returned zero required Vulkan extensions");

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VulkanInstance::populate_debug_messenger_create_info(VkDebugUtilsMessengerCreateInfoEXT& create_info) const 
{
    LOG_METHOD();

    create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanInstance::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    (void)message_type;
    (void)user_data;

    const char* severity_text = "INFO";
    uint32_t color = LoggerPalette::white;

    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_text = "ERROR";
        color = LoggerPalette::error;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_text = "WARNING";
        color = LoggerPalette::yellow;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity_text = "INFO";
        color = LoggerPalette::blue;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity_text = "VERBOSE";
        color = LoggerPalette::gray;
    }

    std::string_view message =
    callback_data && callback_data->pMessage
        ? std::string_view(callback_data->pMessage)
        : std::string_view("<no validation message>");
    
    // Против спама фейковых ошибок
    if (message.find("EOSOverlayVkLayer") != std::string_view::npos) {
        return VK_FALSE;
    }

    std::cerr << clr("[VULKAN ", color)
              << clr(severity_text, color)
              << clr("] ", color)
              << message
              << std::endl;

    return VK_FALSE;
}

VkResult VulkanInstance::create_debug_utils_messenger_ext(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debug_messenger)
{
    LOG_NAMED("VulkanInstance");

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );

    if (func != nullptr) {
        return func(instance, create_info, allocator, debug_messenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanInstance::destroy_debug_utils_messenger_ext(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debug_messenger,
    const VkAllocationCallbacks* allocator)
{
    LOG_NAMED("VulkanInstance");

    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );

    if (func != nullptr) {
        func(instance, debug_messenger, allocator);
    }
}
