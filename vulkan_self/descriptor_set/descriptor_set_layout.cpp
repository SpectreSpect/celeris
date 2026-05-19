#include "descriptor_set_layout.h"
#include <string>

#include "../vulkan_device.h"
#include "descriptor_set_layout_builder.h"
#include <vulkan/vk_enum_string_helper.h>

DescriptorSetLayout::DescriptorSetLayout(const VulkanDevice& device, std::span<const VkDescriptorSetLayoutBinding> bindings
) : m_device(device.handle())  {
    LOG_METHOD();

    logger.check(!bindings.empty(), "DescriptorSetLayout has no bindings. Did you forget to add bindings to DescriptorSetLayoutBuilder?");
    logger.check(bindings.data() != nullptr, "bindings must not be nullptr");
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkResult result = vkCreateDescriptorSetLayout(device.handle(), &layoutInfo, nullptr, &m_layout);

    logger.check(result == VK_SUCCESS) << "Failed to create descriptor set layout: " << clr(string_VkResult(result), LoggerPalette::blue) << "\n";
}

DescriptorSetLayout::DescriptorSetLayout(const VulkanDevice& device, const DescriptorSetLayoutBuilder& builder
) : DescriptorSetLayout(device, builder.get_bindings()) {
}

VkDescriptorSetLayout DescriptorSetLayout::handle() const noexcept {
    return m_layout;
}