#include "vertex_layout_builder.h"

VertexLayoutBuilder& VertexLayoutBuilder::add_binding(
    uint32_t binding,
    uint32_t stride,
    VkVertexInputRate input_rate)
{
    LOG_METHOD();

    VkVertexInputBindingDescription desc{};
    desc.binding = binding;
    desc.stride = stride;
    desc.inputRate = input_rate;

    m_bindings.push_back(desc);
    return *this;
}

VertexLayoutBuilder& VertexLayoutBuilder::add_attribute(
    uint32_t location,
    uint32_t binding,
    VkFormat format,
    uint32_t offset)
{
    LOG_METHOD();

    VkVertexInputAttributeDescription desc{};
    desc.location = location;
    desc.binding = binding;
    desc.format = format;
    desc.offset = offset;

    m_attributes.push_back(desc);
    return *this;
}

const std::vector<VkVertexInputBindingDescription>& VertexLayoutBuilder::bindings() const noexcept {
    return m_bindings;
}

const std::vector<VkVertexInputAttributeDescription>& VertexLayoutBuilder::attributes() const noexcept {
    return m_attributes;
}
