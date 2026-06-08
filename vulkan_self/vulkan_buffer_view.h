#pragma once

#include <cstdint>
#include <span>
#include <vector>
#include <type_traits>
#include <cstddef>
#include <cstring>
#include <optional>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"
#include "vulkan_memory.h"
#include "vulkan_physical_device.h"
#include "vulkan_device.h"

class VulkanCommandBuffer;
class VulkanEngine;
class PassInstance;

class VulkanBufferView {
public:
    _XCLASS_NAME(VulkanBufferView);

    VulkanBufferView() = default;

    VulkanBufferView(VulkanBuffer& vukan_buffer);
    
    bool valid() const noexcept;

    VulkanBuffer& handle() const noexcept;

private:
    VulkanBuffer* m_buffer = nullptr;
};
