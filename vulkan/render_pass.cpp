#include "render_pass.h"
#include "vulkan_utils.h"

#include <array>
#include <vector>

RenderPass::~RenderPass() {
    destroy();
}

RenderPass::RenderPass(RenderPass&& other) noexcept {
    render_pass = other.render_pass;
    device = other.device;

    other.render_pass = VK_NULL_HANDLE;
    other.device = nullptr;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept {
    if (this != &other) {
        destroy();

        render_pass = other.render_pass;
        device = other.device;

        other.render_pass = VK_NULL_HANDLE;
        other.device = nullptr;
    }
    return *this;
}

void RenderPass::destroy() {
    if (device && render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(*device, render_pass, nullptr);
        render_pass = VK_NULL_HANDLE;
    }
}

RenderPassBuilder& RenderPassBuilder::set_color_attachment(const VkAttachmentDescription& attachment) {
    color_attachment = attachment;
    has_color_attachment = true;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::set_depth_attachment(const VkAttachmentDescription& attachment) {
    depth_attachment = attachment;
    has_depth_attachment = true;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::set_dependency(const VkSubpassDependency& dep) {
    dependency = dep;
    has_dependency = true;
    return *this;
}

RenderPassBuilder& RenderPassBuilder::set_default_dependency() {
    has_dependency = true;
    return *this;
}

VkAttachmentDescription RenderPassBuilder::make_present_color_attachment(VkFormat format) {
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    return attachment;
}

VkAttachmentDescription RenderPassBuilder::make_offscreen_color_attachment(VkFormat format) {
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    return attachment;
}

VkAttachmentDescription RenderPassBuilder::make_offscreen_depth_attachment(VkFormat format) {
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return attachment;
}

VkSubpassDependency RenderPassBuilder::make_default_dependency(bool has_depth_attachment) {
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL;
    dep.dstSubpass = 0;

    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.srcAccessMask = 0;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    if (has_depth_attachment) {
        dep.srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dep.dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

    return dep;
}

RenderPass RenderPassBuilder::create(VkDevice& device) const {
    if (!has_color_attachment) {
        throw std::runtime_error("RenderPassBuilder: color attachment is required");
    }

    std::vector<VkAttachmentDescription> attachments;
    attachments.push_back(color_attachment);

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_attachment_ref{};
    if (has_depth_attachment) {
        attachments.push_back(depth_attachment);
        depth_attachment_ref.attachment = 1;
        depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;
    subpass.pDepthStencilAttachment = has_depth_attachment ? &depth_attachment_ref : nullptr;

    VkSubpassDependency dep = has_dependency
        ? dependency
        : make_default_dependency(has_depth_attachment);

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = static_cast<uint32_t>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dep;

    RenderPass out;
    out.device = &device;

    vulkan_utils::vk_check(
        vkCreateRenderPass(device, &render_pass_info, nullptr, &out.render_pass),
        "vkCreateRenderPass"
    );

    return out;
}