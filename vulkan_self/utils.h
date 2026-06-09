#pragma once
#include <cstdint>
#include <concepts>
#include <type_traits>
#include <filesystem>
#include <fstream>
#include <glm/glm.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

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

    template<class Range>
    requires std::ranges::sized_range<Range>
    inline size_t size_bytes(Range&& range) {
        using Elem = std::ranges::range_value_t<Range>;
        return std::ranges::size(range) * sizeof(Elem);
    }

    inline std::vector<std::uint8_t> read_binary_file(const std::filesystem::path& path) {
        LOG_NAMED("Utils");
        
        std::ifstream file(path, std::ios::binary | std::ios::ate);

        logger.check(file.is_open())
            << "Failed to open image file: "
            << clr(path.string(), LoggerPalette::blue)
            << "\n";

        std::streamsize file_size = file.tellg();
        file.seekg(0, std::ios::beg);

        std::vector<std::uint8_t> data(static_cast<size_t>(file_size));

        logger.check(bool(file.read(reinterpret_cast<char*>(data.data()), file_size)))
            << "Failed to read image file: "
            << clr(path.string(), LoggerPalette::blue)
            << "\n";

        return data;
    }

    inline VkDeviceSize format_texel_size_bytes(VkFormat format) {
        switch (format) {
            // 8-bit, 1 component
            case VK_FORMAT_R8_UNORM:
            case VK_FORMAT_R8_SNORM:
            case VK_FORMAT_R8_USCALED:
            case VK_FORMAT_R8_SSCALED:
            case VK_FORMAT_R8_UINT:
            case VK_FORMAT_R8_SINT:
            case VK_FORMAT_R8_SRGB:
            case VK_FORMAT_S8_UINT:
                return 1;

            // 8-bit, 2 components
            case VK_FORMAT_R8G8_UNORM:
            case VK_FORMAT_R8G8_SNORM:
            case VK_FORMAT_R8G8_USCALED:
            case VK_FORMAT_R8G8_SSCALED:
            case VK_FORMAT_R8G8_UINT:
            case VK_FORMAT_R8G8_SINT:
            case VK_FORMAT_R8G8_SRGB:
                return 2;

            // 8-bit, 3 components
            case VK_FORMAT_R8G8B8_UNORM:
            case VK_FORMAT_R8G8B8_SNORM:
            case VK_FORMAT_R8G8B8_USCALED:
            case VK_FORMAT_R8G8B8_SSCALED:
            case VK_FORMAT_R8G8B8_UINT:
            case VK_FORMAT_R8G8B8_SINT:
            case VK_FORMAT_R8G8B8_SRGB:
            case VK_FORMAT_B8G8R8_UNORM:
            case VK_FORMAT_B8G8R8_SNORM:
            case VK_FORMAT_B8G8R8_USCALED:
            case VK_FORMAT_B8G8R8_SSCALED:
            case VK_FORMAT_B8G8R8_UINT:
            case VK_FORMAT_B8G8R8_SINT:
            case VK_FORMAT_B8G8R8_SRGB:
                return 3;

            // 8-bit, 4 components
            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_R8G8B8A8_SNORM:
            case VK_FORMAT_R8G8B8A8_USCALED:
            case VK_FORMAT_R8G8B8A8_SSCALED:
            case VK_FORMAT_R8G8B8A8_UINT:
            case VK_FORMAT_R8G8B8A8_SINT:
            case VK_FORMAT_R8G8B8A8_SRGB:
            case VK_FORMAT_B8G8R8A8_UNORM:
            case VK_FORMAT_B8G8R8A8_SNORM:
            case VK_FORMAT_B8G8R8A8_USCALED:
            case VK_FORMAT_B8G8R8A8_SSCALED:
            case VK_FORMAT_B8G8R8A8_UINT:
            case VK_FORMAT_B8G8R8A8_SINT:
            case VK_FORMAT_B8G8R8A8_SRGB:
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
            case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
            case VK_FORMAT_A8B8G8R8_UINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SINT_PACK32:
            case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
            case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
            case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
            case VK_FORMAT_A2R10G10B10_UINT_PACK32:
            case VK_FORMAT_A2R10G10B10_SINT_PACK32:
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
            case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
            case VK_FORMAT_A2B10G10R10_UINT_PACK32:
            case VK_FORMAT_A2B10G10R10_SINT_PACK32:
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
                return 4;

            // 16-bit, 1 component
            case VK_FORMAT_R16_UNORM:
            case VK_FORMAT_R16_SNORM:
            case VK_FORMAT_R16_USCALED:
            case VK_FORMAT_R16_SSCALED:
            case VK_FORMAT_R16_UINT:
            case VK_FORMAT_R16_SINT:
            case VK_FORMAT_R16_SFLOAT:
            case VK_FORMAT_D16_UNORM:
                return 2;

            // 16-bit, 2 components
            case VK_FORMAT_R16G16_UNORM:
            case VK_FORMAT_R16G16_SNORM:
            case VK_FORMAT_R16G16_USCALED:
            case VK_FORMAT_R16G16_SSCALED:
            case VK_FORMAT_R16G16_UINT:
            case VK_FORMAT_R16G16_SINT:
            case VK_FORMAT_R16G16_SFLOAT:
                return 4;

            // 16-bit, 3 components
            case VK_FORMAT_R16G16B16_UNORM:
            case VK_FORMAT_R16G16B16_SNORM:
            case VK_FORMAT_R16G16B16_USCALED:
            case VK_FORMAT_R16G16B16_SSCALED:
            case VK_FORMAT_R16G16B16_UINT:
            case VK_FORMAT_R16G16B16_SINT:
            case VK_FORMAT_R16G16B16_SFLOAT:
                return 6;

            // 16-bit, 4 components
            case VK_FORMAT_R16G16B16A16_UNORM:
            case VK_FORMAT_R16G16B16A16_SNORM:
            case VK_FORMAT_R16G16B16A16_USCALED:
            case VK_FORMAT_R16G16B16A16_SSCALED:
            case VK_FORMAT_R16G16B16A16_UINT:
            case VK_FORMAT_R16G16B16A16_SINT:
            case VK_FORMAT_R16G16B16A16_SFLOAT:
                return 8;

            // 32-bit, 1 component
            case VK_FORMAT_R32_UINT:
            case VK_FORMAT_R32_SINT:
            case VK_FORMAT_R32_SFLOAT:
            case VK_FORMAT_D32_SFLOAT:
            case VK_FORMAT_X8_D24_UNORM_PACK32:
            case VK_FORMAT_D24_UNORM_S8_UINT:
                return 4;

            // 32-bit, 2 components
            case VK_FORMAT_R32G32_UINT:
            case VK_FORMAT_R32G32_SINT:
            case VK_FORMAT_R32G32_SFLOAT:
                return 8;

            // 32-bit, 3 components
            case VK_FORMAT_R32G32B32_UINT:
            case VK_FORMAT_R32G32B32_SINT:
            case VK_FORMAT_R32G32B32_SFLOAT:
                return 12;

            // 32-bit, 4 components
            case VK_FORMAT_R32G32B32A32_UINT:
            case VK_FORMAT_R32G32B32A32_SINT:
            case VK_FORMAT_R32G32B32A32_SFLOAT:
                return 16;

            // 64-bit, 1 component
            case VK_FORMAT_R64_UINT:
            case VK_FORMAT_R64_SINT:
            case VK_FORMAT_R64_SFLOAT:
                return 8;

            // 64-bit, 2 components
            case VK_FORMAT_R64G64_UINT:
            case VK_FORMAT_R64G64_SINT:
            case VK_FORMAT_R64G64_SFLOAT:
                return 16;

            // 64-bit, 3 components
            case VK_FORMAT_R64G64B64_UINT:
            case VK_FORMAT_R64G64B64_SINT:
            case VK_FORMAT_R64G64B64_SFLOAT:
                return 24;

            // 64-bit, 4 components
            case VK_FORMAT_R64G64B64A64_UINT:
            case VK_FORMAT_R64G64B64A64_SINT:
            case VK_FORMAT_R64G64B64A64_SFLOAT:
                return 32;

            // Depth-stencil special case
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return 8;

            default:
                logger.check(false)
                    << "Unsupported VkFormat in image_size_bytes: "
                    << clr(std::to_string(static_cast<int>(format)), LoggerPalette::orange)
                    << "\n";

                return 0;
        }
    }

    inline uint32_t max_mip_levels(VkExtent3D extent) {
        uint32_t max_dimension = std::max({
            extent.width,
            extent.height,
            extent.depth
        });

        uint32_t levels = 1;

        while (max_dimension > 1) {
            max_dimension /= 2;
            levels++;
        }

        return levels;
    }

    inline VkDeviceSize image_size_bytes(
        VkExtent3D extent, 
        VkFormat format,
        uint32_t mip_levels = 1,
        uint32_t array_layers = 1)
    {
        LOG_NAMED("Utils");

        logger.check(extent.width != 0, "Image width is zero");
        logger.check(extent.height != 0, "Image height is zero");
        logger.check(extent.depth != 0, "Image depth is zero");
        logger.check(format != VK_FORMAT_UNDEFINED, "Image format is undefined");
        logger.check(mip_levels != 0, "Mip levels count is zero");
        logger.check(array_layers != 0, "Array layers count is zero");

        logger.check(mip_levels <= max_mip_levels(extent))
            << "Too many mip levels for image extent "
            << "(" << clr("mip_levels", LoggerPalette::blue) << " = "
            << clr(std::to_string(mip_levels), LoggerPalette::orange)
            << ", max = "
            << clr(std::to_string(max_mip_levels(extent)), LoggerPalette::orange)
            << ")\n";

        const VkDeviceSize texel_size = format_texel_size_bytes(format);

        VkDeviceSize total_size = 0;

        for (uint32_t mip = 0; mip < mip_levels; mip++) {
            const VkDeviceSize mip_width =
                std::max<VkDeviceSize>(1, static_cast<VkDeviceSize>(extent.width) >> mip);

            const VkDeviceSize mip_height =
                std::max<VkDeviceSize>(1, static_cast<VkDeviceSize>(extent.height) >> mip);

            const VkDeviceSize mip_depth =
                std::max<VkDeviceSize>(1, static_cast<VkDeviceSize>(extent.depth) >> mip);

            total_size +=
                mip_width *
                mip_height *
                mip_depth *
                texel_size *
                static_cast<VkDeviceSize>(array_layers);
        }

        return total_size;
    }

    inline uint32_t count_bits(uint32_t value) {
        uint32_t count = 0;

        while (value != 0) {
            count += value & 1u;
            value >>= 1u;
        }

        return count;
    }
}
