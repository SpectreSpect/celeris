#include "descriptor_pool.h"

#include <vulkan/vk_enum_string_helper.h>

#include "../vulkan_device.h"
#include "descriptor_pool_builder.h"
#include "descriptor_set_layout.h"
#include "descriptor_set.h"

DescriptorPool::DescriptorPool(
    const VulkanDevice& device, 
    std::span<const VkDescriptorPoolSize> pool_sizes, 
    uint32_t max_sets,
    VkDescriptorPoolCreateFlags flags) : m_device(device.handle()) {
    LOG_METHOD();

    logger.check(!pool_sizes.empty(), "pool_sizes was empty");
    logger.check(pool_sizes.data() != nullptr, "pool_sizes must not be nullptr");
    logger.check(max_sets > 0, "max_sets must be greater than 0");

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = flags;
    pool_info.poolSizeCount =  static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = max_sets;

    VkResult result = vkCreateDescriptorPool(device.handle(), &pool_info, nullptr, &m_pool);

    logger.check(result == VK_SUCCESS) << "Failed to create descriptor pool: " << clr(string_VkResult(result), LoggerPalette::blue) << "\n";
}

DescriptorPool::DescriptorPool(const VulkanDevice& device, const DescriptorPoolBuilder& builder) 
    :   DescriptorPool(device, builder.pool_sizes(), builder.max_sets(), builder.flags()) {}

DescriptorPool::DescriptorPool(DescriptorPool&& other) noexcept 
    :   m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)),
        m_device(std::exchange(other.m_device, VK_NULL_HANDLE)) {}
    
DescriptorPool& DescriptorPool::operator=(DescriptorPool&& other) noexcept {
    if (this != &other) {
        destory();

        m_pool = std::exchange(m_pool, VK_NULL_HANDLE);
        m_device = std::exchange(m_device, VK_NULL_HANDLE);
    }
    
    return *this;
}

DescriptorPool::~DescriptorPool() {
    destory();
}

void DescriptorPool::destory() noexcept {
    if (m_device != VK_NULL_HANDLE && m_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    }

    m_device = VK_NULL_HANDLE;
    m_pool = VK_NULL_HANDLE;
}

VkDescriptorPool DescriptorPool::handle() const noexcept {
    return m_pool;
}

std::vector<DescriptorSet> DescriptorPool::allocate_sets(const DescriptorSetLayout& layout, uint32_t set_count) {
    LOG_METHOD();

    logger.check(set_count > 0, "set_count must be greater than 0");

    std::vector<VkDescriptorSetLayout> layouts(set_count, layout.handle());

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_pool;
    allocInfo.descriptorSetCount = set_count;
    allocInfo.pSetLayouts = layouts.data();

    std::vector<VkDescriptorSet> descriptor_set_handles(set_count);

    VkResult result = vkAllocateDescriptorSets(m_device, &allocInfo, descriptor_set_handles.data());
    
    logger.check(result == VK_SUCCESS) << "Failed to create descriptor sets: " << clr(string_VkResult(result), LoggerPalette::blue) << "\n";

    std::vector<DescriptorSet> descriptor_sets;
    descriptor_sets.reserve(set_count);

    for (VkDescriptorSet descriptor_set_handle : descriptor_set_handles) {
        descriptor_sets.emplace_back(m_device, descriptor_set_handle);
    }

    return descriptor_sets;
}

DescriptorSet DescriptorPool::allocate_set(const DescriptorSetLayout& layout) {
    LOG_METHOD();

    std::vector<DescriptorSet> descriptor_sets = allocate_sets(layout, 1);

    return descriptor_sets.front();
}