#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/utils.h"
#include "../vulkan_self/vulkan_resource_loader.h"

#include "../vulkan_self/logger/logger_header.h"

class Mesh {
public:
    _XCLASS_NAME(Mesh);

    Mesh(VulkanEngine& engine, VulkanResourceLoader& resource_loader, 
         void* vertex_data, uint32_t vertex_data_size_bytes, 
         unsigned int* index_data, uint32_t index_data_size_bytes);

    uint32_t index_count() const noexcept;
    void bind_vertex_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding = 0, VkDeviceSize offset = 0);
    void bind_index_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding = 0, VkDeviceSize offset = 0);

private:
    VulkanBuffer m_vertex_buffer;
    VulkanBuffer m_index_buffer;
    uint32_t m_index_count = 0;
};