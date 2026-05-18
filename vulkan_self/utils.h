#pragma once
#include <cstdint>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace Utils {
    constexpr uint32_t align_up(uint32_t value, uint32_t alignment) {
        return (value + alignment - 1) / alignment * alignment;
    }

    constexpr VkDeviceSize align_up(VkDeviceSize value, VkDeviceSize alignment) {
        return (value + alignment - 1) / alignment * alignment;
    }

    constexpr VkDeviceSize align_down(VkDeviceSize value, VkDeviceSize alignment) {
        return value / alignment * alignment;
    }    

    inline glm::vec2 to_vec2(VkExtent2D extent) {
        return glm::vec2{
            static_cast<float>(extent.width), 
            static_cast<float>(extent.height)
        };
    }

    inline glm::vec2 to_vec2(VkOffset2D offset) {
        return glm::vec2{
            static_cast<float>(offset.x), 
            static_cast<float>(offset.y)
        };
    }
}
