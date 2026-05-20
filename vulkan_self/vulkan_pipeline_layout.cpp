#include "vulkan_pipeline_layout.h"

#include "vulkan_device.h"
#include "vulkan_command_buffer.h"
#include "descriptor_set/descriptor_set_layout.h"

#include <utility>

PipelineLayoutBuilder& PipelineLayoutBuilder::set_device(const VulkanDevice& device) noexcept {
    m_desc.device = device.handle();
    return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::add_push_constants(
    uint32_t size_bytes,
    uint32_t offset,
    VkShaderStageFlags stage_flags) 
{
    LOG_METHOD();

    logger.check(size_bytes > 0)
        << "Push constant range size must be greater than zero\n";

    logger.check(size_bytes % 4 == 0)
        << "Push constant range size must be a multiple of 4\n";

    logger.check(offset % 4 == 0)
        << "Push constant range offset must be a multiple of 4\n";

    logger.check(stage_flags != 0)
        << "Push constant range stage flags must not be zero\n";

    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = stage_flags;
    push_constant_range.offset = offset;
    push_constant_range.size = size_bytes;

    m_desc.push_constant_ranges.push_back(push_constant_range);

    return *this;
}

PipelineLayoutBuilder& PipelineLayoutBuilder::add_descriptor_set_layout(const DescriptorSetLayout& layout) {
    LOG_METHOD();

    m_desc.descriptor_set_layouts.push_back(layout.handle());

    return *this;
}


const PipelineLayoutBuilderDesc& PipelineLayoutBuilder::desc() const noexcept {
    return m_desc;
}

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

VulkanPipelineLayout::VulkanPipelineLayout(const PipelineLayoutBuilder& builder) : m_device(builder.desc().device)
{
    LOG_METHOD();

    logger.check(m_device != VK_NULL_HANDLE)
        << "Device is not initialized. Init it or specify in the method "
        << VSCODE_CLR_STREAM("PipelineLayoutBuilder", "set_device") << "\n";

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    
    if (!builder.desc().descriptor_set_layouts.empty()) {
        pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(builder.desc().descriptor_set_layouts.size());
        pipeline_layout_info.pSetLayouts = builder.desc().descriptor_set_layouts.data();
    } else {
        pipeline_layout_info.setLayoutCount = 0;
        pipeline_layout_info.pSetLayouts = nullptr;
    }

    pipeline_layout_info.pushConstantRangeCount = static_cast<uint32_t>(builder.desc().push_constant_ranges.size());
    pipeline_layout_info.pPushConstantRanges = builder.desc().push_constant_ranges.data();

    push_constant_ranges = builder.desc().push_constant_ranges; // Копирование

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
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)), 
        push_constant_ranges(std::move(other.push_constant_ranges)) {}

VulkanPipelineLayout& VulkanPipelineLayout::operator=(VulkanPipelineLayout&& other) noexcept {
    if (this != &other) {
        destroy();

        m_pipeline_layout = std::exchange(other.m_pipeline_layout, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        push_constant_ranges = std::move(other.push_constant_ranges);
    }

    return *this;
}

VkPipelineLayout VulkanPipelineLayout::handle() const noexcept {
    return m_pipeline_layout;
}

PipelineLayoutBuilder VulkanPipelineLayout::create_builder() noexcept {
    return PipelineLayoutBuilder();
}
