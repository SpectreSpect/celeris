#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>

class RenderPass {
public:
    VkRenderPass render_pass = VK_NULL_HANDLE;
    VkDevice* device = nullptr;

    RenderPass() = default;
    ~RenderPass();

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    RenderPass(RenderPass&& other) noexcept;
    RenderPass& operator=(RenderPass&& other) noexcept;

    void destroy();
};

class RenderPassBuilder {
public:
    VkAttachmentDescription color_attachment{};
    VkAttachmentDescription depth_attachment{};

    bool has_color_attachment = false;
    bool has_depth_attachment = false;

    VkSubpassDependency dependency{};
    bool has_dependency = false;

    RenderPassBuilder() = default;

    RenderPassBuilder& set_color_attachment(const VkAttachmentDescription& attachment);
    RenderPassBuilder& set_depth_attachment(const VkAttachmentDescription& attachment);
    RenderPassBuilder& set_dependency(const VkSubpassDependency& dep);

    RenderPassBuilder& set_default_dependency();

    RenderPass create(VkDevice& device) const;

    static VkAttachmentDescription make_present_color_attachment(VkFormat format);
    static VkAttachmentDescription make_offscreen_color_attachment(VkFormat format);
    static VkAttachmentDescription make_offscreen_depth_attachment(VkFormat format);

    static VkSubpassDependency make_default_dependency(bool has_depth_attachment);
};