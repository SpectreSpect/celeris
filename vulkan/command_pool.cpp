#include "command_pool.h"
#include "vulkan_utils.h"

CommandPool::CommandPool(VkDevice* device, VkPhysicalDevice* physical_device, VkQueue* queue, uint32_t queue_family_id)
    :   device(device),
        physical_device(physical_device),
        queue(queue),
        queue_family_id(queue_family_id)
{
    if (device == nullptr) {
        std::string message = "CommandPool::CommandPool: device was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (physical_device == nullptr) {
        std::string message = "CommandPool::CommandPool: physical_device was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (queue == nullptr) {
        std::string message = "CommandPool::CommandPool: queue was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queue_family_id;

    vulkan_utils::vk_check(
        vkCreateCommandPool(*device, &poolInfo, nullptr, &pool),
        "vkCreateCommandPool"
    );
}

CommandPool::~CommandPool() {
    destroy();
}

void CommandPool::destroy() {
    if (pool != VK_NULL_HANDLE && device == nullptr) {
        std::string message = 
            "CommandPool::destroy: Memory leak due to inability to destroy object "
            "- device is nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (pool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(*device, pool, nullptr);
        pool = VK_NULL_HANDLE;
    }
}

CommandPool::CommandPool(CommandPool&& other) noexcept
    :   device(std::exchange(other.device, nullptr)),
        physical_device(std::exchange(other.physical_device, nullptr)),
        queue(std::exchange(other.queue, VK_NULL_HANDLE)),
        pool(std::exchange(other.pool, VK_NULL_HANDLE)),
        queue_family_id(std::exchange(other.queue_family_id, 0))
{

}

CommandPool& CommandPool::operator=(CommandPool&& other) noexcept {
    if (this == &other)
        return *this;
    
    destroy();

    device = std::exchange(other.device, nullptr);
    physical_device = std::exchange(other.physical_device, nullptr);
    queue = std::exchange(other.queue, VK_NULL_HANDLE);
    pool = std::exchange(other.pool, VK_NULL_HANDLE);
    queue_family_id = std::exchange(other.queue_family_id, 0);
}