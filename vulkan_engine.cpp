#include "vulkan_engine.h"
#include "vulkan/graphics_pipeline.h"

namespace {
    constexpr const char* kDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    void vk_check(VkResult res, const char* what) {
        if (res != VK_SUCCESS) {
            throw std::runtime_error(std::string(what) + " failed with VkResult = " + std::to_string(res));
        }
    }
}

VulkanEngine::VulkanEngine() {
    init();
}

VulkanEngine::~VulkanEngine() {
    cleanup();
}

int VulkanEngine::init() {
    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return -1;
    }

    // Vulkan: no OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    return 1;
}

// void VulkanEngine::set_window(Window* window) {
//     // if (!window || !window->window) {
//     //     std::cerr << "set_window: GLFWwindow is null\n";
//     //     return;
//     // }

//     // m_window = window;

//     // try {
//     //     createInstance();
//     //     createSurface();
//     //     pickPhysicalDevice();
//     //     createLogicalDevice();

//     //     // Needed for actual rendering to the window:
//     //     createSwapchain();
//     //     createImageViews();

//     //     std::cout << "Vulkan initialized successfully\n";
//     // } catch (const std::exception& e) {
//     //     std::cerr << "Vulkan init failed: " << e.what() << "\n";
//     // }
// }

void VulkanEngine::set_vulkan_window(VulkanWindow* window) {
    if (!window || !window->window) {
        std::cerr << "set_window: GLFWwindow is null\n";
        return;
    }

    m_window = window;

    try {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapchain();
        createImageViews();

        createDepthResources(
            device,
            physicalDevice,
            swapchainExtent,
            depthImage,
            depthImageMemory,
            depthImageView,
            depthFormat
        );

        createRenderPass();
        createCommandPool();
        create_framebuffers();
        create_command_buffers();
        create_sync_objects();
        create_imgui_descriptor_pool();

        std::cout << "Vulkan initialized successfully\n";
    } catch (const std::exception& e) {
        std::cerr << "Vulkan init failed: " << e.what() << "\n";
    }
}

void VulkanEngine::poll_events() {
    glfwPollEvents();
}

void VulkanEngine::enable_depth_test() {
    // No-op in Vulkan.
    // Depth testing is configured in VkPipelineDepthStencilStateCreateInfo.
}

uint32_t VulkanEngine::get_graphics_queue_family() const {
    return findQueueFamilies(physicalDevice).graphicsFamily.value();
}

uint32_t VulkanEngine::get_imgui_min_image_count() const {
    return imguiMinImageCount;
}

void VulkanEngine::imgui_check_vk_result(VkResult err) {
    if (err == VK_SUCCESS) {
        return;
    }

    std::cerr << "[imgui/vulkan] VkResult = " << err << "\n";
    if (err < 0) {
        std::abort();
    }
}

VkDescriptorPool VulkanEngine::get_imgui_descriptor_pool() const {
    return imguiDescriptorPool;
}

VkFormat VulkanEngine::findSupportedFormat(
    VkPhysicalDevice physicalDevice,
    const std::vector<VkFormat>& candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        }

        if (tiling == VK_IMAGE_TILING_OPTIMAL &&
            (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format");
}

VkFormat VulkanEngine::findDepthFormat(VkPhysicalDevice physicalDevice) {
    return findSupportedFormat(
        physicalDevice,
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT
        },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

bool VulkanEngine::hasStencilComponent(VkFormat format) {
    return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           format == VK_FORMAT_D24_UNORM_S8_UINT;
}

uint32_t VulkanEngine::findMemoryType(
    VkPhysicalDevice physicalDevice,
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type");
}

void VulkanEngine::createImage(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkImage& image,
    VkDeviceMemory& imageMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VulkanEngine::vk_check(
        vkCreateImage(device, &imageInfo, nullptr, &image),
        "vkCreateImage"
    );

    VkMemoryRequirements memRequirements{};
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = VulkanEngine::findMemoryType(
        physicalDevice,
        memRequirements.memoryTypeBits,
        properties
    );

    VulkanEngine::vk_check(
        vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory),
        "vkAllocateMemory(depth image)"
    );

    VulkanEngine::vk_check(
        vkBindImageMemory(device, image, imageMemory, 0),
        "vkBindImageMemory"
    );
}

VkImageView VulkanEngine::createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    VulkanEngine::vk_check(
        vkCreateImageView(device, &viewInfo, nullptr, &imageView),
        "vkCreateImageView(depth)"
    );
    return imageView;
}

void VulkanEngine::createDepthResources(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkExtent2D extent,
    VkImage& depthImage,
    VkDeviceMemory& depthImageMemory,
    VkImageView& depthImageView,
    VkFormat& depthFormat)
{
    depthFormat = findDepthFormat(physicalDevice);

    createImage(
        device,
        physicalDevice,
        extent.width,
        extent.height,
        depthFormat,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        depthImage,
        depthImageMemory
    );

    depthImageView = createImageView(
        device,
        depthImage,
        depthFormat,
        hasStencilComponent(depthFormat)
            ? (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
            : VK_IMAGE_ASPECT_DEPTH_BIT
    );
}

// void VulkanEngine::bind_descriptor_set(DescriptorSet& descriptor_set) {
//     vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
//                             graphics_pipeline.pipeline_layout, 0, 1, &graphics_pipeline.descriptor_set.descriptor_set, 0, nullptr);
// }

void VulkanEngine::bind_pipeline(GraphicsPipeline& graphics_pipeline) {
    vkCmdBindPipeline(
            currentCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            graphics_pipeline.pipeline
        );

    vkCmdBindDescriptorSets(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, 
                            graphics_pipeline.pipeline_layout, 0, 1, &graphics_pipeline.descriptor_set_bundle->descriptor_set.descriptor_set, 0, nullptr);
}

void VulkanEngine::bind_vertex_buffer(VideoBuffer& vertex_buffer) {
    VkBuffer vertexBuffers[] = {vertex_buffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(currentCommandBuffer, 0, 1, vertexBuffers, offsets);
}

void VulkanEngine::bind_vertex_and_instance_buffers(VideoBuffer& vertex_buffer, VideoBuffer& instance_buffer) {
    VkBuffer vertexBuffers[] = {vertex_buffer.buffer, instance_buffer.buffer};
    VkDeviceSize offsets[] = {0, 0};
    vkCmdBindVertexBuffers(currentCommandBuffer, 0, 2, vertexBuffers, offsets);
}


void VulkanEngine::bind_index_buffer(VideoBuffer& index_buffer) {        
    vkCmdBindIndexBuffer(currentCommandBuffer, index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanEngine::draw_indexed(uint32_t num_indices) {
    vkCmdDrawIndexed(currentCommandBuffer, num_indices, 1, 0, 0, 0);
}

// void VulkanEngine::draw(Drawable& drawable, Camera& camera) {
//     RenderState render_state = {};

//     float aspect = float(swapchainExtent.width) / float(swapchainExtent.height);
//     render_state.proj = camera.get_projection_matrix(aspect);
//     render_state.vp = render_state.proj * camera.get_view_matrix();
//     render_state.engine = this;
//     render_state.transform = glm::mat4(1.0f);
//     render_state.graphics_pipeline = &graphics_pipeline;
//     render_state.camera = &camera;
//     render_state.vp = render_state.proj * camera.get_view_matrix();

//     drawable.draw(render_state);
// }

void VulkanEngine::begin_frame(const glm::vec4& clear_color) {
    if (frameInProgress) {
        std::string message = "VulkanEngine::begin_frame: begin_frame called while a frame is already in progress";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE) {
        return;
    }

    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(m_window->window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0) {
        // Window is minimized or not drawable right now.
        return;
    }

    vk_check(vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX),
             "vkWaitForFences");

    VkResult acquireRes = vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,
        imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &currentImageIndex
    );

    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR) {
        recreate_swapchain();
        return;
    }

    if (acquireRes != VK_SUCCESS && acquireRes != VK_SUBOPTIMAL_KHR) {
        std::string message = "VulkanEngine::begin_frame: vkAcquireNextImageKHR failed";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    vk_check(vkResetFences(device, 1, &inFlightFence), "vkResetFences");

    currentCommandBuffer = commandBuffers[currentImageIndex];

    vk_check(vkResetCommandBuffer(currentCommandBuffer, 0), "vkResetCommandBuffer");

    

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vk_check(vkBeginCommandBuffer(currentCommandBuffer, &beginInfo),
             "vkBeginCommandBuffer");

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color.float32[0] = clear_color.r;
    clearValues[0].color.float32[1] = clear_color.g;
    clearValues[0].color.float32[2] = clear_color.b;
    clearValues[0].color.float32[3] = clear_color.a;
    clearValues[1].depthStencil.depth = 1.0f;
    clearValues[1].depthStencil.stencil = 0;

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = swapchainFramebuffers[currentImageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapchainExtent;
    rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    rpInfo.pClearValues = clearValues.data();

    set_viewport_and_scissor({(unsigned int)fbW, (unsigned int)fbH});

    vkCmdBeginRenderPass(currentCommandBuffer, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Optional: if your pipeline uses dynamic viewport/scissor, keep these.
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;
    vkCmdSetScissor(currentCommandBuffer, 0, 1, &scissor);

    frameInProgress = true;
}

void VulkanEngine::end_frame() {
    if (!frameInProgress) {
        return;
    }

    vkCmdEndRenderPass(currentCommandBuffer);
    vk_check(vkEndCommandBuffer(currentCommandBuffer), "vkEndCommandBuffer");

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &currentCommandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

    vk_check(vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence),
             "vkQueueSubmit");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &currentImageIndex;

    VkResult presentRes = vkQueuePresentKHR(presentQueue, &presentInfo);

    frameInProgress = false;
    currentCommandBuffer = VK_NULL_HANDLE;

    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR ||
        presentRes == VK_SUBOPTIMAL_KHR ||
        framebufferResized) {
        framebufferResized = false;
        recreate_swapchain();
        return;
    }

    if (presentRes != VK_SUCCESS) {
        throw std::runtime_error("vkQueuePresentKHR failed");
    }
}

void VulkanEngine::on_framebuffer_resized(int w, int h) {
    framebufferResized = true;

    // If minimized, wait until begin_frame sees a non-zero framebuffer.
    if (w == 0 || h == 0) {
        return;
    }

    // Usually better to defer actual recreation until end_frame/begin_frame,
    // not directly inside the GLFW callback.
}

void VulkanEngine::recreate_swapchain() {
    int width = 0;
    int height = 0;

    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device);

    destroy_swapchain_dependent_objects();

    createSwapchain();
    createImageViews();

    // If your render pass depends on swapchain format, recreate it too.
    // createRenderPass();

    create_framebuffers();
    create_command_buffers();
    ui::set_min_image_count(imguiMinImageCount);
}

void VulkanEngine::destroy_swapchain_dependent_objects() {
    for (VkFramebuffer fb : swapchainFramebuffers) {
        if (fb != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, fb, nullptr);
        }
    }
    swapchainFramebuffers.clear();

    if (!commandBuffers.empty()) {
        vkFreeCommandBuffers(
            device,
            commandPool,
            static_cast<uint32_t>(commandBuffers.size()),
            commandBuffers.data()
        );
        commandBuffers.clear();
    }

    for (VkImageView view : swapchainImageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyImageView(device, view, nullptr);
        }
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void VulkanEngine::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {
        colorAttachment,
        depthAttachment
    };

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    vk_check(
        vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass),
        "vkCreateRenderPass"
    );
}

void VulkanEngine::createCommandPool() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

    vk_check(
        vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool),
        "vkCreateCommandPool"
    );
}

void VulkanEngine::create_framebuffers() {
    swapchainFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); ++i) {
        VkImageView attachments[] = {
            swapchainImageViews[i],
            depthImageView
        };

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = swapchainExtent.width;
        fbInfo.height = swapchainExtent.height;
        fbInfo.layers = 1;

        vk_check(vkCreateFramebuffer(device, &fbInfo, nullptr, &swapchainFramebuffers[i]),
                 "vkCreateFramebuffer");
    }
}

void VulkanEngine::create_command_buffers() {
    commandBuffers.resize(swapchainImages.size());

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    vk_check(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),
             "vkAllocateCommandBuffers");
}


void VulkanEngine::create_imgui_descriptor_pool() {
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 },
    };

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 64;
    pool_info.poolSizeCount = static_cast<uint32_t>(std::size(pool_sizes));
    pool_info.pPoolSizes = pool_sizes;

    vk_check(
        vkCreateDescriptorPool(device, &pool_info, nullptr, &imguiDescriptorPool),
        "vkCreateDescriptorPool(imgui)"
    );
}

void VulkanEngine::create_sync_objects() {
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    vk_check(vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailableSemaphore),
             "vkCreateSemaphore(imageAvailableSemaphore)");
    vk_check(vkCreateSemaphore(device, &semInfo, nullptr, &renderFinishedSemaphore),
             "vkCreateSemaphore(renderFinishedSemaphore)");
    vk_check(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence),
             "vkCreateFence");
}


void VulkanEngine::cleanup() {
    for (VkImageView view : swapchainImageViews) {
        if (view != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
            imguiDescriptorPool = VK_NULL_HANDLE;
        }
    }
    swapchainImageViews.clear();

    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }

    if (imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, imguiDescriptorPool, nullptr);
        imguiDescriptorPool = VK_NULL_HANDLE;
    }

    if (device != VK_NULL_HANDLE) {
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
    }

    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }

    if (instance != VK_NULL_HANDLE) {
        vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
    }
    

    glfwTerminate();
}

std::vector<const char*> VulkanEngine::getRequiredInstanceExtensions() const {
    uint32_t count = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&count);
    if (!glfwExts || count == 0) {
        throw std::runtime_error("glfwGetRequiredInstanceExtensions returned nothing");
    }

    return std::vector<const char*>(glfwExts, glfwExts + count);
}

void VulkanEngine::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "My Vulkan App";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "VulkanEngine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    auto extensions = getRequiredInstanceExtensions();

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    vk_check(vkCreateInstance(&createInfo, nullptr, &instance), "vkCreateInstance");
}

void VulkanEngine::createSurface() {
    vk_check(
        glfwCreateWindowSurface(instance, m_window->window, nullptr, &surface),
        "glfwCreateWindowSurface"
    );
}

VulkanEngine::QueueFamilyIndices VulkanEngine::findQueueFamilies(VkPhysicalDevice dev) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool VulkanEngine::checkDeviceExtensionSupport(VkPhysicalDevice dev) const {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, available.data());

    std::set<std::string> required(std::begin(kDeviceExtensions), std::end(kDeviceExtensions));

    for (const auto& ext : available) {
        required.erase(ext.extensionName);
    }

    return required.empty();
}

bool VulkanEngine::isDeviceSuitable(VkPhysicalDevice dev) const {
    QueueFamilyIndices indices = findQueueFamilies(dev);
    bool extensionsSupported = checkDeviceExtensionSupport(dev);

    return indices.isComplete() && extensionsSupported;
}

void VulkanEngine::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vk_check(vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");

    if (deviceCount == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vk_check(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice dev : devices) {
        if (isDeviceSuitable(dev)) {
            physicalDevice = dev;
            break;
        }
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("No suitable Vulkan GPU found");
    }
}

void VulkanEngine::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(std::size(kDeviceExtensions));
    createInfo.ppEnabledExtensionNames = kDeviceExtensions;

    vk_check(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device), "vkCreateDevice");

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

// -------- Minimal swapchain setup --------

void VulkanEngine::createSwapchain() {
    VkSurfaceCapabilitiesKHR capabilities{};
    vk_check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities),
             "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    uint32_t formatCount = 0;
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(count)");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vk_check(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data()),
             "vkGetPhysicalDeviceSurfaceFormatsKHR(list)");

    uint32_t presentModeCount = 0;
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(count)");
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vk_check(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data()),
             "vkGetPhysicalDeviceSurfacePresentModesKHR(list)");

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = pm;
            break;
        }
    }

    VkExtent2D extent = capabilities.currentExtent;

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    vk_check(vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain), "vkCreateSwapchainKHR");

    vk_check(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr),
             "vkGetSwapchainImagesKHR(count)");
    swapchainImages.resize(imageCount);
    vk_check(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data()),
             "vkGetSwapchainImagesKHR(list)");

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
    imguiMinImageCount = imageCount;
}

void VulkanEngine::createImageViews() {
    swapchainImageViews.resize(swapchainImages.size());

    for (size_t i = 0; i < swapchainImages.size(); ++i) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        vk_check(vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]),
                 "vkCreateImageView");
    }
}


void VulkanEngine::set_viewport_and_scissor(VkExtent2D extent) {
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(currentCommandBuffer, 0, 1, &scissor);
}

// VkQueue VulkanEngine::get_compute_queue() {
//     VkQueue computeQueue;
//     vkGetDeviceQueue(device, computeQueueFamilyIndex, 0, &computeQueue);
//     return computeQueue;
// }