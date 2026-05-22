#include "vulkan_render_pass.h"
#include "vulkan_device.h"
#include "vulkan_swapchain.h"
#include "vulkan_framebuffer.h"
#include "vulkan_command_buffer.h"

#include <utility>

RenderPassScope::RenderPassScope(
    VulkanRenderPass& render_pass,
    VulkanCommandBuffer& command_buffer,
    const VulkanFramebuffer& framebuffer,
    VkExtent2D extent,
    VkOffset2D offset,
    VkClearValue clear_color)
    :   m_render_pass(render_pass),
        m_command_buffer(command_buffer)
{
    m_render_pass.begin(command_buffer, framebuffer, extent, offset, clear_color);
}

RenderPassScope::~RenderPassScope() noexcept {
    m_render_pass.end(m_command_buffer);
}

VulkanRenderPass::VulkanRenderPass(const VulkanDevice& device, const VkRenderPassCreateInfo& desc)
    :   m_device(device.handle())
{
    LOG_METHOD();
    create(desc);
}

VulkanRenderPass::VulkanRenderPass(const VulkanDevice& device, const VulkanSwapchain& swapchain, VkFormat depth_format)
    :   m_device(device.handle())
{
    LOG_METHOD();

    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain.image_format();
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::array<VkAttachmentDescription, 2> attachments = {
        color_attachment,
        depth_attachment
    };

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

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

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    create(render_pass_info);
}

VulkanRenderPass::~VulkanRenderPass() {
    destroy();
}

void VulkanRenderPass::destroy() {
    if (m_device != VK_NULL_HANDLE && m_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    }

    m_render_pass = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

VulkanRenderPass::VulkanRenderPass(VulkanRenderPass&& other) noexcept 
    :   m_render_pass(std::exchange(other.m_render_pass, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanRenderPass& VulkanRenderPass::operator=(VulkanRenderPass&& other) noexcept 
{
    if (this != &other) {
        destroy();

        m_render_pass = std::exchange(other.m_render_pass, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkRenderPass VulkanRenderPass::handle() const noexcept {
    return m_render_pass;
}

void VulkanRenderPass::begin(
    VulkanCommandBuffer& command_buffer,
    const VulkanFramebuffer& framebuffer,
    VkExtent2D extent,
    VkOffset2D offset,
    VkClearValue clear_color)
{
    LOG_METHOD();

    std::array<VkClearValue, 2> clear_values{};

    clear_values[0] = clear_color;

    clear_values[1].depthStencil = {
        1.0f, // depth
        0     // stencil
    };

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = framebuffer.handle();

    render_pass_info.renderArea.offset = offset;
    render_pass_info.renderArea.extent = extent;

    render_pass_info.clearValueCount = static_cast<uint32_t>(clear_values.size());
    render_pass_info.pClearValues = clear_values.data();

    vkCmdBeginRenderPass(
        command_buffer.handle(),
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );
}

void VulkanRenderPass::begin(
    VulkanCommandBuffer& command_buffer,
    const VulkanFramebuffer& framebuffer,
    const VulkanSwapchain& swapchain,
    VkClearValue clear_color)
{
    begin(command_buffer, framebuffer, swapchain.extent(), {0, 0}, clear_color);
}


void VulkanRenderPass::end(VulkanCommandBuffer& command_buffer) {
    LOG_METHOD();
    vkCmdEndRenderPass(command_buffer.handle());
}

RenderPassScope VulkanRenderPass::begin_scope(
    VulkanCommandBuffer& command_buffer,
    const VulkanFramebuffer& framebuffer,
    VkExtent2D extent,
    VkOffset2D offset,
    VkClearValue clear_color)
{
    LOG_METHOD();
    return RenderPassScope(*this, command_buffer, framebuffer, extent, offset, clear_color);
}

RenderPassScope VulkanRenderPass::begin_scope(
    VulkanCommandBuffer& command_buffer,
    const VulkanFramebuffer& framebuffer,
    const VulkanSwapchain& swapchain,
    VkClearValue clear_color)
{
    LOG_METHOD();
    return RenderPassScope(*this, command_buffer, framebuffer, swapchain.extent(), {0, 0}, clear_color);
}

void VulkanRenderPass::create(const VkRenderPassCreateInfo& desc) {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");

    VkResult result = vkCreateRenderPass(
        m_device,
        &desc,
        nullptr,
        &m_render_pass
    );

    logger.check(result == VK_SUCCESS, "Failed to create render pass");
}
