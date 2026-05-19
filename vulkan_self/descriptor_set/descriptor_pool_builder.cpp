#include "descriptor_pool_builder.h"
#include "descriptor_set_layout_builder.h"

DescriptorPoolBuilder& DescriptorPoolBuilder::add_descriptors(VkDescriptorType type, uint32_t descriptor_count) {
    LOG_METHOD();

    logger.check(descriptor_count > 0, "descriptor_count must be greater than 0");

    for (VkDescriptorPoolSize& pool_size : m_pool_sizes) {
        if (pool_size.type == type) {
            pool_size.descriptorCount += descriptor_count;
            return *this;
        }
    }

    VkDescriptorPoolSize pool_size{};
    pool_size.type = type;
    pool_size.descriptorCount = descriptor_count;

    m_pool_sizes.push_back(pool_size);

    return *this;
}

DescriptorPoolBuilder& DescriptorPoolBuilder::add_layout(const DescriptorSetLayoutBuilder& layout_builder, uint32_t set_count) {
    LOG_METHOD();

    logger.check(set_count, "set_count must be greater than 0");

    std::span<const VkDescriptorSetLayoutBinding> bindings = layout_builder.get_bindings();

    logger.check(!bindings.empty(), "DescriptorSetLayout has no bindings. Did you forget to add bindings to DescriptorSetLayoutBuilder?");
    logger.check(bindings.data() != nullptr, "DescriptorPoolBuilder received a non-empty bindings span with nullptr data");

    for (const VkDescriptorSetLayoutBinding binding : bindings) {
        add_descriptors(binding.descriptorType, binding.descriptorCount * set_count);
    }

    m_max_sets += set_count;

    return *this;
}

std::span<const VkDescriptorPoolSize> DescriptorPoolBuilder::pool_sizes() const noexcept {
    return m_pool_sizes;
}

uint32_t DescriptorPoolBuilder::max_sets() const noexcept {
    return m_max_sets;
}