#include "fence.h"

Fence::Fence(VkDevice* device, bool signaled)
    :   device(device)
{

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

    if (vkCreateFence(*device, &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        std::string message = "Fence::Fence: failed to create fence.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }
}

Fence::~Fence() {
    destroy();
}

void Fence::destroy() {
    if (fence != VK_NULL_HANDLE && device == nullptr) {
        std::string message = 
            "Fence::destroy: Memory leak due to inability to destroy object "
            "- device is nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }
    
    if (fence != VK_NULL_HANDLE) {
        vkDestroyFence(*device, fence, nullptr);
        fence = VK_NULL_HANDLE;
    }

    device = nullptr;
}

Fence::Fence(Fence&& other) noexcept
    :   device(std::exchange(other.device, nullptr)),
        fence(std::exchange(other.fence, VK_NULL_HANDLE))
{

}

Fence& Fence::operator=(Fence&& other) noexcept {
    if (this == &other)
        return *this;
    
    destroy();

    device = std::exchange(other.device, nullptr);
    fence = std::exchange(other.fence, VK_NULL_HANDLE);
}

void Fence::reset() {
    if (device == nullptr) {
        std::string message = "Fence::reset: device was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (vkResetFences(*device, 1, &fence) != VK_SUCCESS) {
        std::string message = "Fence::reset: failed to reset fence.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }
}

void Fence::wait_for_fence() {
    if (device == nullptr) {
        std::string message = "Fence::wait_for_fence: device was nullptr.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (vkWaitForFences(*device, 1, &fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        std::string message = "Fence::wait_for_fence: failed to wait for fence.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }
}