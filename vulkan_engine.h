#pragma once

#include <set>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "drawable.h"
#include "imgui_layer.h"
#include "vulkan_window.h"

class GraphicsPipeline;
class VideoBuffer;

class VulkanEngine {
public:
    VulkanWindow* m_window = nullptr;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent{};

    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> swapchainFramebuffers;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;

    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;

    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    uint32_t currentImageIndex = 0;
    VkCommandBuffer currentCommandBuffer = VK_NULL_HANDLE;
    
    uint32_t imguiMinImageCount = 2;

    bool frameInProgress = false;
    bool framebufferResized = false;

    VulkanEngine();
    ~VulkanEngine();

    int init();
    // void set_window(Window* window);
    void set_vulkan_window(VulkanWindow* window);
    void poll_events();
    void enable_depth_test();

    virtual void begin_frame(const glm::vec4& clear_color);
    virtual void end_frame();
    virtual void on_framebuffer_resized(int w, int h);

    void recreate_swapchain();
    void create_framebuffers();
    void create_command_buffers();
    void create_sync_objects();
    void destroy_swapchain_dependent_objects();
    void createRenderPass();
    void createCommandPool();
    static uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    static VkFormat findSupportedFormat(VkPhysicalDevice physicalDevice, const std::vector<VkFormat>& candidates, 
                                        VkImageTiling tiling, VkFormatFeatureFlags features);
    static VkFormat findDepthFormat(VkPhysicalDevice physicalDevice);
    static bool hasStencilComponent(VkFormat format);
    static void createImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format, 
                            VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                            VkImage& image, VkDeviceMemory& imageMemory);
    static VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
    static void createDepthResources(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkImage& depthImage, 
                                     VkDeviceMemory& depthImageMemory, VkImageView& depthImageView, VkFormat& depthFormat);
    
    // void bind_descriptor_set(DescriptorSet& descriptor_set);
    void bind_pipeline(GraphicsPipeline& graphics_pipeline);
    void bind_vertex_buffer(VideoBuffer& vertex_buffer);
    void bind_vertex_and_instance_buffers(VideoBuffer& vertex_buffer, VideoBuffer& instance_buffer);
    void bind_index_buffer(VideoBuffer& index_buffer);
    void draw_indexed(uint32_t num_indices);
    void set_viewport_and_scissor(VkExtent2D extent);
    VkQueue get_compute_queue(); 

    uint32_t get_graphics_queue_family() const;
    uint32_t get_imgui_min_image_count() const;
    VkDescriptorPool get_imgui_descriptor_pool() const;

    static void imgui_check_vk_result(VkResult err);

    // void draw(Drawable& drawable, Camera& camera);

    // vkCmdDrawIndexed(engine.currentCommandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    static void vk_check(VkResult res, const char* what) {
        if (res != VK_SUCCESS) {
            throw std::runtime_error(std::string(what) + " failed with VkResult = " + std::to_string(res));
        }
    }

    VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    void create_imgui_descriptor_pool();

private:
    void cleanup();

    void createInstance();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();

    // Needed if you want actual window rendering:
    void createSwapchain();
    void createImageViews();

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice dev) const;
    bool isDeviceSuitable(VkPhysicalDevice dev) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice dev) const;

    std::vector<const char*> getRequiredInstanceExtensions() const;
    
};