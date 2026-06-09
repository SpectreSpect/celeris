#pragma once
#include <string>
#include <utility>
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>


class Fence {
public:
    VkDevice* device = nullptr;
    VkFence fence = VK_NULL_HANDLE;

    Fence() = default;
    Fence(VkDevice* device, bool signaled = false);
    
    ~Fence();
    void destroy();

    Fence(const Fence&) = delete;
    Fence& operator=(const Fence&) = delete;

    Fence(Fence&& other) noexcept;
    Fence& operator=(Fence&& other) noexcept;

    void reset();
    void wait_for_fence();
};