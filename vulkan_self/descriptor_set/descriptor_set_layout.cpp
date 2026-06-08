#include "descriptor_set_layout.h"
#include <string>

#include "../vulkan_device.h"
#include "descriptor_set_layout_builder.h"
#include <vulkan/vk_enum_string_helper.h>

DescriptorSetLayout::DescriptorSetLayout(const VulkanDevice& device, std::span<const VkDescriptorSetLayoutBinding> bindings, VkDescriptorSetLayoutCreateFlags flags
) : m_device(device.handle())  {
    LOG_METHOD();

    logger.check(!bindings.empty(), "DescriptorSetLayout has no bindings. Did you forget to add bindings to DescriptorSetLayoutBuilder?");
    logger.check(bindings.data() != nullptr, "bindings must not be nullptr");
    
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    layoutInfo.flags = flags;

    VkResult result = vkCreateDescriptorSetLayout(device.handle(), &layoutInfo, nullptr, &m_layout);

    logger.check(result == VK_SUCCESS) << "Failed to create descriptor set layout: " << clr(string_VkResult(result), LoggerPalette::blue) << "\n";
}

DescriptorSetLayout::DescriptorSetLayout(const VulkanDevice& device, const DescriptorSetLayoutBuilder& builder
) : DescriptorSetLayout(device, builder.get_bindings(), builder.flags()) {}

DescriptorSetLayout::DescriptorSetLayout(DescriptorSetLayout&& other) noexcept
    :   m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}


DescriptorSetLayout& DescriptorSetLayout::operator=(DescriptorSetLayout&& other) noexcept {
    if (this != &other) {
        destory();

        m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    }

    return *this;
}

DescriptorSetLayout::~DescriptorSetLayout() noexcept{
    destory();
}

void DescriptorSetLayout::destory() noexcept {
    if (m_device != VK_NULL_HANDLE && m_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    }

    m_layout = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
}

VkDescriptorSetLayout DescriptorSetLayout::handle() const noexcept {
    return m_layout;
}