#include "imgui_layer.h"

#include <cassert>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "vulkan_self/vulkan_engine.h"
#include "vulkan_self/window.h"
#include "vulkan_self/vulkan_command_buffer.h"
#include "vulkan_self/descriptor_set/descriptor_pool_builder.h"

#include <vulkan/vk_enum_string_helper.h>

UI::UI(Window& window, VulkanEngine& engine, const VulkanInitInfo& info) : m_descriptor_pool(create_descriptor_pool(engine))
{
    LOG_METHOD();

    init_imgui(window, engine, info);
}

UI::UI(Window& window, VulkanEngine& engine) : m_descriptor_pool(create_descriptor_pool(engine)) {
    LOG_METHOD();

    VulkanInitInfo info = get_default_vulkan_init_info(engine, m_descriptor_pool);

    init_imgui(window, engine, info);
}

UI::~UI() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void UI::begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UI::update_mouse_mode(Window& window) {
    ImGuiIO& io = ImGui::GetIO();

    if (window.mouse_state().mode == MouseMode::NORMAL)
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    else
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void UI::end_frame(VulkanCommandBuffer& command_buffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer.handle());
}

// void UI::set_min_image_count(uint32_t min_image_count) {

// }

DescriptorPool UI::create_descriptor_pool(VulkanEngine& engine) const {
    LOG_METHOD();

    DescriptorPoolBuilder descriptor_pool_builder;
    descriptor_pool_builder.add_descriptors(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64);
    descriptor_pool_builder.set_max_sets(64);
    descriptor_pool_builder.set_flags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

    return DescriptorPool(engine.device(), descriptor_pool_builder);
}

UI::VulkanInitInfo UI::get_default_vulkan_init_info(VulkanEngine& engine, DescriptorPool& descriptor_pool) {
    LOG_NAMED("UI");
    
    VulkanInitInfo ui_info{};
    ui_info.instance = engine.instance().handle();
    ui_info.physical_device = engine.physical_device().handle();
    ui_info.device = engine.device().handle();
    ui_info.queue_family = engine.graphics_queue(1).family_info().queue_family_index;
    ui_info.queue = engine.graphics_queue(1).handle();
    ui_info.descriptor_pool = descriptor_pool.handle();
    ui_info.render_pass = engine.swapchain_resources().render_pass.handle();
    ui_info.min_image_count = 2;
    ui_info.image_count = static_cast<uint32_t>(engine.swapchain_resources().swapchain.images().size());
    ui_info.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    ui_info.pipeline_cache = VK_NULL_HANDLE;
    ui_info.subpass = 0;
    ui_info.check_vk_result_fn = check_vk_result;

    return ui_info;
}

void UI::check_vk_result(VkResult err) {
    LOG_NAMED("UI");

    logger.check(err == VK_SUCCESS) << clr(string_VkResult(err), LoggerPalette::blue) << "\n";

    if (err < 0)
        std::abort();
}

void UI::init_imgui(Window& window, VulkanEngine& engine, const VulkanInitInfo& info) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    logger.check(ImGui_ImplGlfw_InitForVulkan(window.handle(), true), "Failed to initialize ImGui GLFW backend");

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = info.instance;
    init_info.PhysicalDevice = info.physical_device;
    init_info.Device = info.device;
    init_info.QueueFamily = info.queue_family;
    init_info.Queue = info.queue;
    init_info.DescriptorPool = info.descriptor_pool;
    init_info.RenderPass = info.render_pass;
    init_info.MinImageCount = info.min_image_count;
    init_info.ImageCount = info.image_count;
    init_info.MSAASamples = info.msaa_samples;
    init_info.PipelineCache = info.pipeline_cache;
    init_info.Subpass = info.subpass;
    init_info.Allocator = info.allocator;
    init_info.CheckVkResultFn = info.check_vk_result_fn;

    logger.check(ImGui_ImplVulkan_Init(&init_info), "Failed to initialize ImGui Vulkan backend");
}



// namespace ui {
//     void init(GLFWwindow* window, const VulkanInitInfo& info) {
//         IMGUI_CHECKVERSION();
//         ImGui::CreateContext();

//         ImGuiIO& io = ImGui::GetIO();
//         io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

//         ImGui::StyleColorsDark();

//         const bool glfw_ok = ImGui_ImplGlfw_InitForVulkan(window, true);
//         assert(glfw_ok && "ImGui_ImplGlfw_InitForVulkan failed");

//         ImGui_ImplVulkan_InitInfo init_info{};
//         init_info.Instance = info.instance;
//         init_info.PhysicalDevice = info.physical_device;
//         init_info.Device = info.device;
//         init_info.QueueFamily = info.queue_family;
//         init_info.Queue = info.queue;
//         init_info.DescriptorPool = info.descriptor_pool;
//         init_info.RenderPass = info.render_pass;
//         init_info.MinImageCount = info.min_image_count;
//         init_info.ImageCount = info.image_count;
//         init_info.MSAASamples = info.msaa_samples;
//         init_info.PipelineCache = info.pipeline_cache;
//         init_info.Subpass = info.subpass;
//         init_info.Allocator = info.allocator;
//         init_info.CheckVkResultFn = info.check_vk_result_fn;

//         const bool vulkan_ok = ImGui_ImplVulkan_Init(&init_info);
//         assert(vulkan_ok && "ImGui_ImplVulkan_Init failed");
//     }


//     void init(Window& window, VulkanEngine& engine, DescriptorPool& descriptor_pool) {
//         ui::VulkanInitInfo ui_info{};
//         ui_info.instance = engine.instance().handle();
//         ui_info.physical_device = engine.physical_device().handle();
//         ui_info.device = engine.device().handle();
//         ui_info.queue_family = engine.graphics_queue(1).family_info().queue_family_index;
//         ui_info.queue = engine.graphics_queue(1).handle();
//         ui_info.descriptor_pool = descriptor_pool.handle();
//         ui_info.render_pass = engine.swapchain_resources().render_pass.handle();
//         ui_info.min_image_count = 2;
//         ui_info.image_count = static_cast<uint32_t>(engine.swapchain_resources().swapchain.images().size());
//         ui_info.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
//         ui_info.pipeline_cache = VK_NULL_HANDLE;
//         ui_info.subpass = 0;
//         ui_info.check_vk_result_fn = check_vk_result;

//         ui::init(window.handle(), ui_info);
//     }

//     void check_vk_result(VkResult err) {
//         if (err == VK_SUCCESS) {
//             return;
//         }

//         std::cerr << "[imgui/vulkan] VkResult = " << err << "\n";
//         if (err < 0) {
//             std::abort();
//         }
//     }

//     void begin_frame() {
//         ImGui_ImplVulkan_NewFrame();
//         ImGui_ImplGlfw_NewFrame();
//         ImGui::NewFrame();
//     }

//     void update_mouse_mode(Window& window) {
//         ImGuiIO& io = ImGui::GetIO();

//         if (window.mouse_state().mode == MouseMode::NORMAL)
//             io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
//         else
//             io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
//     }

//     void end_frame(VkCommandBuffer command_buffer) {
//         ImGui::Render();
//         ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
//     }

//     void end_frame(VulkanCommandBuffer& command_buffer) {
//         ImGui::Render();
//         ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer.handle());
//     }

//     void set_min_image_count(uint32_t min_image_count) {
//         ImGui_ImplVulkan_SetMinImageCount(min_image_count);
//     }

//     void shutdown() {
//         ImGui_ImplVulkan_Shutdown();
//         ImGui_ImplGlfw_Shutdown();
//         ImGui::DestroyContext();
//     }
// }