#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"

class CpuImage {
public:
    _XCLASS_NAME(CpuImage);

    explicit CpuImage(
        std::vector<std::uint8_t> image_data,
        VkExtent3D extent,
        VkFormat format
    );
    
    VkExtent3D extent() const noexcept;
    VkExtent2D extent2d() const noexcept;
    VkFormat format() const noexcept;
    VkDeviceSize size_bytes() const;

    std::vector<uint8_t>& image_data();
    const std::vector<uint8_t>& image_data() const;

    static CpuImage load_rgba8_image(const std::filesystem::path& path, VkFormat format = VK_FORMAT_R8G8B8A8_SRGB);

private:
    std::vector<uint8_t> m_image_data;

    VkExtent3D m_extent = {0, 0, 0};
    VkFormat m_format = VK_FORMAT_UNDEFINED;
};
