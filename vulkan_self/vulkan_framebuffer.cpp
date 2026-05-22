#include "vulkan_framebuffer.h"
#include "vulkan_device.h"
#include "vulkan_render_pass.h"
#include "image/vulkan_image_view.h"

#include <utility>

VulkanFramebuffer::VulkanFramebuffer(const VulkanDevice& device, const VkFramebufferCreateInfo& desc) 
    :   m_device(device.handle()) 
{
    LOG_METHOD();
    create(desc);
}

VulkanFramebuffer::VulkanFramebuffer(
    const VulkanDevice& device,
    const VulkanRenderPass& render_pass,
    std::span<const VulkanImageView> attachments,
    VkExtent2D extent2d,
    uint32_t layers) : m_device(device.handle())
{
    LOG_METHOD();

    std::vector<VkImageView> handle_attachments;
    handle_attachments.reserve(attachments.size());

    for (const VulkanImageView& image_view : attachments) {
        handle_attachments.push_back(image_view.handle());
    }

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass.handle();
    framebuffer_info.attachmentCount = static_cast<uint32_t>(handle_attachments.size());
    framebuffer_info.pAttachments = handle_attachments.data();
    framebuffer_info.width = extent2d.width;
    framebuffer_info.height = extent2d.height;
    framebuffer_info.layers = layers;

    create(framebuffer_info);
}

VulkanFramebuffer::~VulkanFramebuffer() {
    destroy();
}

void VulkanFramebuffer::destroy() {
    if (m_device != VK_NULL_HANDLE && m_framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(m_device, m_framebuffer, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_framebuffer = VK_NULL_HANDLE;
}

VulkanFramebuffer::VulkanFramebuffer(VulkanFramebuffer&& other) noexcept
    :   m_framebuffer(std::exchange(other.m_framebuffer, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanFramebuffer& VulkanFramebuffer::operator=(VulkanFramebuffer&& other) noexcept {
    if (this != &other) {
        destroy();

        m_framebuffer = std::exchange(other.m_framebuffer, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkFramebuffer VulkanFramebuffer::handle() const noexcept {
    return m_framebuffer;
}

void VulkanFramebuffer::create(const VkFramebufferCreateInfo& desc) {
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE, "Device is not initialized");
    
    VkResult result = vkCreateFramebuffer(
        m_device,
        &desc,
        nullptr,
        &m_framebuffer
    );

    logger.check(result == VK_SUCCESS, "Failed to create framebuffer");
}

std::vector<VulkanFramebuffer> VulkanFramebuffer::from_image_views(
    std::span<const VulkanImageView> image_views,
    const VulkanDevice& device,
    const VulkanRenderPass& render_pass,
    VkExtent2D extent2d,
    uint32_t layers)
{
    LOG_NAMED("VulkanFramebuffer");

    std::vector<VulkanFramebuffer> framebuffers;
    framebuffers.reserve(image_views.size());

    for (const VulkanImageView& image_view : image_views) {
        framebuffers.emplace_back(
            device,
            render_pass,
            std::span{&image_view, 1}, 
            extent2d,
            layers
        );
    }

    return framebuffers;
}
