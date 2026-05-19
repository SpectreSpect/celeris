#pragma once

#include <vector>
#include <string_view>
#include <memory>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_physical_device.h"
#include "vulkan_queue.h"

class VulkanDevice {
public:
    _XCLASS_NAME(VulkanDevice);

    explicit VulkanDevice(const VulkanPhysicalDevice& physical_device);
    ~VulkanDevice();
    void destroy();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;

    VkDevice handle() const noexcept;
    void wait_idle();

    const VulkanQueue& graphics_queue(uint32_t index = 0) const;
    VulkanQueue& graphics_queue(uint32_t index = 0);

    const VulkanQueue& present_queue(uint32_t index = 0) const;
    VulkanQueue& present_queue(uint32_t index = 0);
    
    const VulkanQueue& compute_queue(uint32_t index = 0) const;
    VulkanQueue& compute_queue(uint32_t index = 0);
    
    const VulkanQueue& transfer_queue(uint32_t index = 0) const;
    VulkanQueue& transfer_queue(uint32_t index = 0);
    

private:
    VkDevice m_device = VK_NULL_HANDLE;

    std::vector<std::unique_ptr<VulkanQueue>> m_graphics_queues;
    std::vector<std::unique_ptr<VulkanQueue>> m_present_queues; 
    std::vector<std::unique_ptr<VulkanQueue>> m_compute_queues;
    std::vector<std::unique_ptr<VulkanQueue>> m_transfer_queues;

private:
    void retrieve_queues(const QueueAllocation& queue_allocation);
    
    const VulkanQueue& get_queue(
        const std::vector<std::unique_ptr<VulkanQueue>>& queues, 
        uint32_t index, 
        std::string_view error_message
    ) const;

    VulkanQueue& get_queue(
        std::vector<std::unique_ptr<VulkanQueue>>& queues, 
        uint32_t index, 
        std::string_view error_message
    );
};
