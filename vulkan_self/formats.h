#pragma once

#include <vulkan/vulkan.h>

namespace Formats {
    // Undefined
    constexpr VkFormat undefined = VK_FORMAT_UNDEFINED;

    // 8-bit unsigned normalized
    constexpr VkFormat r8_unorm = VK_FORMAT_R8_UNORM;
    constexpr VkFormat r8g8_unorm = VK_FORMAT_R8G8_UNORM;
    constexpr VkFormat r8g8b8_unorm = VK_FORMAT_R8G8B8_UNORM;
    constexpr VkFormat r8g8b8a8_unorm = VK_FORMAT_R8G8B8A8_UNORM;

    // 8-bit signed normalized
    constexpr VkFormat r8_snorm = VK_FORMAT_R8_SNORM;
    constexpr VkFormat r8g8_snorm = VK_FORMAT_R8G8_SNORM;
    constexpr VkFormat r8g8b8_snorm = VK_FORMAT_R8G8B8_SNORM;
    constexpr VkFormat r8g8b8a8_snorm = VK_FORMAT_R8G8B8A8_SNORM;

    // 8-bit unsigned integer
    constexpr VkFormat r8_uint = VK_FORMAT_R8_UINT;
    constexpr VkFormat r8g8_uint = VK_FORMAT_R8G8_UINT;
    constexpr VkFormat r8g8b8_uint = VK_FORMAT_R8G8B8_UINT;
    constexpr VkFormat r8g8b8a8_uint = VK_FORMAT_R8G8B8A8_UINT;

    // 8-bit signed integer
    constexpr VkFormat r8_sint = VK_FORMAT_R8_SINT;
    constexpr VkFormat r8g8_sint = VK_FORMAT_R8G8_SINT;
    constexpr VkFormat r8g8b8_sint = VK_FORMAT_R8G8B8_SINT;
    constexpr VkFormat r8g8b8a8_sint = VK_FORMAT_R8G8B8A8_SINT;

    // 8-bit sRGB
    constexpr VkFormat r8_srgb = VK_FORMAT_R8_SRGB;
    constexpr VkFormat r8g8_srgb = VK_FORMAT_R8G8_SRGB;
    constexpr VkFormat r8g8b8_srgb = VK_FORMAT_R8G8B8_SRGB;
    constexpr VkFormat r8g8b8a8_srgb = VK_FORMAT_R8G8B8A8_SRGB;

    // BGR / BGRA color formats
    constexpr VkFormat b8g8r8_unorm = VK_FORMAT_B8G8R8_UNORM;
    constexpr VkFormat b8g8r8a8_unorm = VK_FORMAT_B8G8R8A8_UNORM;
    constexpr VkFormat b8g8r8_srgb = VK_FORMAT_B8G8R8_SRGB;
    constexpr VkFormat b8g8r8a8_srgb = VK_FORMAT_B8G8R8A8_SRGB;

    // 16-bit floating point
    constexpr VkFormat r16_sfloat = VK_FORMAT_R16_SFLOAT;
    constexpr VkFormat r16g16_sfloat = VK_FORMAT_R16G16_SFLOAT;
    constexpr VkFormat r16g16b16_sfloat = VK_FORMAT_R16G16B16_SFLOAT;
    constexpr VkFormat r16g16b16a16_sfloat = VK_FORMAT_R16G16B16A16_SFLOAT;

    // 16-bit unsigned integer
    constexpr VkFormat r16_uint = VK_FORMAT_R16_UINT;
    constexpr VkFormat r16g16_uint = VK_FORMAT_R16G16_UINT;
    constexpr VkFormat r16g16b16_uint = VK_FORMAT_R16G16B16_UINT;
    constexpr VkFormat r16g16b16a16_uint = VK_FORMAT_R16G16B16A16_UINT;

    // 16-bit signed integer
    constexpr VkFormat r16_sint = VK_FORMAT_R16_SINT;
    constexpr VkFormat r16g16_sint = VK_FORMAT_R16G16_SINT;
    constexpr VkFormat r16g16b16_sint = VK_FORMAT_R16G16B16_SINT;
    constexpr VkFormat r16g16b16a16_sint = VK_FORMAT_R16G16B16A16_SINT;

    // 32-bit floating point
    constexpr VkFormat r32_sfloat = VK_FORMAT_R32_SFLOAT;
    constexpr VkFormat r32g32_sfloat = VK_FORMAT_R32G32_SFLOAT;
    constexpr VkFormat r32g32b32_sfloat = VK_FORMAT_R32G32B32_SFLOAT;
    constexpr VkFormat r32g32b32a32_sfloat = VK_FORMAT_R32G32B32A32_SFLOAT;

    // 32-bit unsigned integer
    constexpr VkFormat r32_uint = VK_FORMAT_R32_UINT;
    constexpr VkFormat r32g32_uint = VK_FORMAT_R32G32_UINT;
    constexpr VkFormat r32g32b32_uint = VK_FORMAT_R32G32B32_UINT;
    constexpr VkFormat r32g32b32a32_uint = VK_FORMAT_R32G32B32A32_UINT;

    // 32-bit signed integer
    constexpr VkFormat r32_sint = VK_FORMAT_R32_SINT;
    constexpr VkFormat r32g32_sint = VK_FORMAT_R32G32_SINT;
    constexpr VkFormat r32g32b32_sint = VK_FORMAT_R32G32B32_SINT;
    constexpr VkFormat r32g32b32a32_sint = VK_FORMAT_R32G32B32A32_SINT;

    // 64-bit floating point
    constexpr VkFormat r64_sfloat = VK_FORMAT_R64_SFLOAT;
    constexpr VkFormat r64g64_sfloat = VK_FORMAT_R64G64_SFLOAT;
    constexpr VkFormat r64g64b64_sfloat = VK_FORMAT_R64G64B64_SFLOAT;
    constexpr VkFormat r64g64b64a64_sfloat = VK_FORMAT_R64G64B64A64_SFLOAT;

    // Common packed formats
    constexpr VkFormat a2b10g10r10_unorm_pack32 = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    constexpr VkFormat a2b10g10r10_uint_pack32 = VK_FORMAT_A2B10G10R10_UINT_PACK32;
    constexpr VkFormat a2r10g10b10_unorm_pack32 = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    constexpr VkFormat a2r10g10b10_uint_pack32 = VK_FORMAT_A2R10G10B10_UINT_PACK32;

    // Depth / stencil
    constexpr VkFormat d16_unorm = VK_FORMAT_D16_UNORM;
    constexpr VkFormat d32_sfloat = VK_FORMAT_D32_SFLOAT;
    constexpr VkFormat s8_uint = VK_FORMAT_S8_UINT;
    constexpr VkFormat d16_unorm_s8_uint = VK_FORMAT_D16_UNORM_S8_UINT;
    constexpr VkFormat d24_unorm_s8_uint = VK_FORMAT_D24_UNORM_S8_UINT;
    constexpr VkFormat d32_sfloat_s8_uint = VK_FORMAT_D32_SFLOAT_S8_UINT;

    // Common aliases for engine usage
    constexpr VkFormat vec2 = VK_FORMAT_R32G32_SFLOAT;
    constexpr VkFormat vec3 = VK_FORMAT_R32G32B32_SFLOAT;
    constexpr VkFormat vec4 = VK_FORMAT_R32G32B32A32_SFLOAT;

    constexpr VkFormat color_srgb = VK_FORMAT_R8G8B8A8_SRGB;
    constexpr VkFormat color_unorm = VK_FORMAT_R8G8B8A8_UNORM;
    constexpr VkFormat swapchain_color = VK_FORMAT_B8G8R8A8_SRGB;

    constexpr VkFormat depth = VK_FORMAT_D32_SFLOAT;
    constexpr VkFormat depth_stencil = VK_FORMAT_D24_UNORM_S8_UINT;
}