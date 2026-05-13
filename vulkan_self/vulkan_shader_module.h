#pragma once

#include <vector>
#include <string>
#include <string_view>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanDevice;

class VulkanShaderModule {
public:
    _XCLASS_NAME(VulkanShaderModule);

    explicit VulkanShaderModule(const VulkanDevice& device, std::string_view file_path);
    ~VulkanShaderModule() noexcept;
    void destroy() noexcept;

    VulkanShaderModule(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;

    VulkanShaderModule(VulkanShaderModule&& other) noexcept;
    VulkanShaderModule& operator=(VulkanShaderModule&& other) noexcept;

    VkShaderModule handle() const noexcept;

private:
    VkShaderModule m_shader_module = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    static std::vector<char> read_file(std::string_view filename);
};
