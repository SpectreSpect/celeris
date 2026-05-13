#include "vulkan_pipeline_layout.h"

#include "vulkan_device.h"

#include <utility>

VulkanPipelineLayout::VulkanPipelineLayout(const VulkanDevice& device) : m_device(device.handle()) {
    LOG_METHOD();

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    VkResult result = vkCreatePipelineLayout(
        m_device,
        &pipeline_layout_info,
        nullptr,
        &m_pipeline_layout
    );

    logger.check(result == VK_SUCCESS, "Failed to create pipeline layout");
}

VulkanPipelineLayout::~VulkanPipelineLayout() noexcept {
    destroy();
}

void VulkanPipelineLayout::destroy() noexcept {
    if (m_device != VK_NULL_HANDLE && m_pipeline_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_pipeline_layout = VK_NULL_HANDLE;
}

VulkanPipelineLayout::VulkanPipelineLayout(VulkanPipelineLayout&& other) noexcept
    :   m_pipeline_layout(std::exchange(other.m_pipeline_layout, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}

VulkanPipelineLayout& VulkanPipelineLayout::operator=(VulkanPipelineLayout&& other) noexcept {
    if (this != &other) {
        destroy();

        m_pipeline_layout = std::exchange(other.m_pipeline_layout, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

VkPipelineLayout VulkanPipelineLayout::handle() const noexcept {
    return m_pipeline_layout;
}
