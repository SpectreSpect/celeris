#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <stdexcept>

class LayoutInitializer;

enum AttrFormat {
    FLOAT2 = VK_FORMAT_R32G32_SFLOAT,
    FLOAT4 = VK_FORMAT_R32G32B32A32_SFLOAT
};

class VulkanVertexLayout {
public:
    std::vector<VkVertexInputAttributeDescription> attribute_descs{};
    uint32_t total_size = 0u;
    uint32_t binding = 0u;

    VulkanVertexLayout() = default;
    LayoutInitializer get_initializer(uint32_t start_location = 0);
    void add(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
    VkPipelineVertexInputStateCreateInfo get_vertex_intput();
private:
    VkVertexInputBindingDescription binding_desc{};
};

class LayoutInitializer {
public:
    VulkanVertexLayout* vertex_layout = nullptr;
    uint32_t current_location = 0;
    uint32_t current_offset = 0;

    inline LayoutInitializer(VulkanVertexLayout& vertex_layout, uint32_t start_location = 0) {
        this->vertex_layout = &vertex_layout;
        this->current_location = start_location;
    };

    void add(VkFormat format) {
        if (!vertex_layout)
            throw "vertex_layout was nullptr";

        VkVertexInputAttributeDescription attribute_desc = {};
        attribute_desc.location = current_location;
        attribute_desc.binding = vertex_layout->binding;
        attribute_desc.format = format;
        attribute_desc.offset = current_offset;

        vertex_layout->attribute_descs.push_back(attribute_desc);

        // uint32_t size = 0;
        // if (format == VK_FORMAT_R32G32B32A32_SFLOAT)
        //     size = 16;
        // else if (format == VK_FORMAT_R32G32_SFLOAT)
        //     size = 8;
        // else
        //     throw std::runtime_error("This type hasn't been added yet");
        
        uint32_t size = 0;
        switch (format) {
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                size = 16;
                break;
            case VK_FORMAT_R32G32_SFLOAT:
                size = 8;
                break;
            default: 
                throw std::runtime_error("This type hasn't been added yet");
        }

        current_location++;
        current_offset += size;
        vertex_layout->total_size += size;
    }

    void add(AttrFormat format) {
        VkFormat vk_format;

        switch (format) {
            case AttrFormat::FLOAT4:
                vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            case AttrFormat::FLOAT2:
                vk_format = VK_FORMAT_R32G32_SFLOAT;
                break;
            default:
                throw std::runtime_error("This type hasn't been added yet");
        }
        
        add(vk_format);
    }
};
