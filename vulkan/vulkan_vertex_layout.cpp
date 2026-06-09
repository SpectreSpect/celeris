#include "vulkan_vertex_layout.h"


// void VertexLayout::create() {
//     VkVertexInputBindingDescription bindingDesc{};
//     bindingDesc.binding = 0;
//     bindingDesc.stride = sizeof(Vertex);
//     bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

//     std::array<VkVertexInputAttributeDescription, 3> attrDescs{};

//     attrDescs[0].location = 0;
//     attrDescs[0].binding = 0;
//     attrDescs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
//     attrDescs[0].offset = offsetof(Vertex, position);

//     attrDescs[1].location = 1;
//     attrDescs[1].binding = 0;
//     attrDescs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
//     attrDescs[1].offset = offsetof(Vertex, normal);

//     attrDescs[2].location = 2;
//     attrDescs[2].binding = 0;
//     attrDescs[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
//     attrDescs[2].offset = offsetof(Vertex, color);

//     VkPipelineVertexInputStateCreateInfo vertexInput{};
//     vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
//     vertexInput.vertexBindingDescriptionCount = 1;
//     vertexInput.pVertexBindingDescriptions = &bindingDesc;
//     vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
//     vertexInput.pVertexAttributeDescriptions = attrDescs.data();
// }

LayoutInitializer VulkanVertexLayout::get_initializer(uint32_t start_location) {
    return LayoutInitializer(*this, start_location);
}


void VulkanVertexLayout::add(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    VkVertexInputAttributeDescription attribute_desc = {};
    attribute_desc.location = location;
    attribute_desc.binding = binding;
    attribute_desc.format = format;
    attribute_desc.offset = offset;

    attribute_descs.push_back(attribute_desc);
}

VkPipelineVertexInputStateCreateInfo VulkanVertexLayout::get_vertex_intput() {
    binding_desc.binding = binding;
    binding_desc.stride = total_size;
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding_desc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attribute_descs.size());
    vertexInput.pVertexAttributeDescriptions = attribute_descs.data();

    return vertexInput;
}