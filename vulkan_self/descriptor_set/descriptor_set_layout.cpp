#include "descriptor_set_layout.h"
#include "../vulkan_device.h"
#include <string>

DescriptorSetLayout::DescriptorSetLayout(const VulkanDevice& device, uint32_t binding_count, const VkDescriptorSetLayoutBinding* bindings) {
    LOG_METHOD();
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = binding_count;
    layoutInfo.pBindings = bindings;

    VkResult result = vkCreateDescriptorSetLayout(device.handle(), &layoutInfo, nullptr, &layout);

    logger.check(result == VK_SUCCESS) << "Failed to create descriptor set layout: " << clr(std::to_string(result), LoggerPalette::blue);
}

VkDescriptorSetLayout DescriptorSetLayout::handle() const noexcept {
    return layout;
}