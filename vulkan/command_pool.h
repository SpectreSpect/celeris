#pragma once
#include <stdexcept>
#include <utility>
#include <vulkan/vulkan.h>

class CommandPool {
public:
    VkDevice* device = nullptr;
    VkPhysicalDevice* physical_device = nullptr;
    VkQueue* queue = VK_NULL_HANDLE;

    VkCommandPool pool = VK_NULL_HANDLE;
    uint32_t queue_family_id = 0;

    CommandPool() = default;
    CommandPool(VkDevice* device, VkPhysicalDevice* physical_device, VkQueue* queue, uint32_t queue_family_id);

    ~CommandPool();
    void destroy();

    CommandPool(const CommandPool&) = delete;
    CommandPool& operator=(const CommandPool&) = delete;

    CommandPool(CommandPool&& other) noexcept;
    CommandPool& operator=(CommandPool&& other) noexcept;
};