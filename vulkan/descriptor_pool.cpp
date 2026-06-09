#include "descriptor_pool.h"


void DescriptorPool::create(VkDevice& device, DescriptorSetLayout& layout, uint32_t num_sets) {
    std::unordered_map<VkDescriptorType, uint32_t> counts;

    for (const auto& binding : layout.descriptor_set_bindings) {
        counts[binding.descriptorType] += binding.descriptorCount * num_sets;
    }

    std::vector<VkDescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(counts.size());

    for (const auto& [type, count] : counts) {
        VkDescriptorPoolSize pool_size{};
        pool_size.type = type;
        pool_size.descriptorCount = count;
        pool_sizes.push_back(pool_size);
    }

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    pool_info.maxSets = num_sets;

    vulkan_utils::vk_check(
        vkCreateDescriptorPool(device, &pool_info, nullptr, &pool),
        "vkCreateDescriptorPool"
    );
}
