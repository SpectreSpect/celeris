#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VertexLayoutBuilder {
public:
    _XCLASS_NAME(VertexLayoutBuilder);

    VertexLayoutBuilder() = default;

    // Должен быть минимум один биндинг! #TODO
    VertexLayoutBuilder& add_binding(
        uint32_t binding,
        uint32_t stride,
        VkVertexInputRate input_rate = VK_VERTEX_INPUT_RATE_VERTEX
    );

    VertexLayoutBuilder& add_attribute(
        uint32_t location,
        uint32_t binding,
        VkFormat format,
        uint32_t offset
    );

    const std::vector<VkVertexInputBindingDescription>& bindings() const noexcept;
    const std::vector<VkVertexInputAttributeDescription>& attributes() const noexcept;

private:
    std::vector<VkVertexInputBindingDescription> m_bindings;
    std::vector<VkVertexInputAttributeDescription> m_attributes;
};
