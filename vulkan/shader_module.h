#pragma once
#include <filesystem>
#include <vulkan/vulkan.h>
#include <vector>
#include <fstream>
#include "vulkan_utils.h"
#include <filesystem>

class VulkanShaderModule {
public:
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VkDevice* device = nullptr;

    VulkanShaderModule() = default;
    VulkanShaderModule(VkDevice& device, const std::filesystem::path& path);

    ~VulkanShaderModule();

    VulkanShaderModule(const VulkanShaderModule&) = delete;
    VulkanShaderModule& operator=(const VulkanShaderModule&) = delete;

    VulkanShaderModule(VulkanShaderModule&& other) noexcept;
    VulkanShaderModule& operator=(VulkanShaderModule&& other) noexcept;

    std::vector<char> read_file(const std::filesystem::path& filename);
    void create(VkDevice& device, const std::vector<char>& code);
    void create(VkDevice& device, const std::filesystem::path& filename);
    void destroy();
};