#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/vulkan_buffer_view.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/utils.h"
#include "../vulkan_self/vulkan_resource_loader.h"

#include "../vulkan_self/logger/logger_header.h"

class VulkanPhysicalDevice;
class VulkanDevice;

// const VulkanPhysicalDevice& physical_device,
// const VulkanDevice& device,

class MeshView {
public:
    _XCLASS_NAME(MeshView);

    MeshView() = default;

    MeshView(VulkanBufferView vertex_buffer_view, VulkanBufferView index_buffer_view, uint32_t index_count);

    // MeshView(VulkanPhysicalDevice& physical_device, VulkanDevice& device, VulkanResourceLoader& resource_loader, 
    //         void* vertex_data, uint32_t vertex_data_size_bytes, 
    //         unsigned int* index_data, uint32_t index_data_size_bytes);

    // MeshView(VulkanEngine& engine, VulkanResourceLoader& resource_loader, 
    //      void* vertex_data, uint32_t vertex_data_size_bytes, 
    //      unsigned int* index_data, uint32_t index_data_size_bytes);

    VulkanBufferView& vertex_buffer_view() noexcept;
    const VulkanBufferView& vertex_buffer_view() const noexcept;

    VulkanBufferView& index_buffer_view() noexcept;
    const VulkanBufferView& index_buffer_view() const noexcept;

    uint32_t index_count() const noexcept;

    void bind_vertex_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding = 0, VkDeviceSize offset = 0);
    void bind_index_buffer(VulkanCommandBuffer& command_buffer, uint32_t buffer_binding = 0, VkDeviceSize offset = 0);
    
    bool valid() const noexcept;

private:
    VulkanBufferView m_vertex_buffer_view;
    VulkanBufferView m_index_buffer_view;
    uint32_t m_index_count = 0;
};