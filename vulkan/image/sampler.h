#pragma once
#include <utility>

#include <vulkan/vulkan.h>
#include "../../vulkan_engine.h"

class Sampler {
public:
    struct SamplerCreateDesc {
        VkFilter mag_filter = VK_FILTER_LINEAR;
        VkFilter min_filter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        float mip_lod_bias = 0.0f;

        VkBool32 anisotropy_enable = VK_FALSE;
        float max_anisotropy = 1.0f;

        VkBool32 compare_enable = VK_FALSE;
        VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;

        float min_lod = 0.0f;
        float max_lod = 0.0f;

        VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        VkBool32 unnormalized_coordinates = VK_FALSE;
    };

    VulkanEngine* engine;

    VkSampler sampler = VK_NULL_HANDLE;

    VkFilter mag_filter = VK_FILTER_LINEAR;
    VkFilter min_filter = VK_FILTER_LINEAR;
    VkSamplerMipmapMode mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSamplerAddressMode address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    float mip_lod_bias = 0.0f;

    VkBool32 anisotropy_enable = VK_FALSE;
    float max_anisotropy = 1.0f;

    VkBool32 compare_enable = VK_FALSE;
    VkCompareOp compare_op = VK_COMPARE_OP_ALWAYS;

    float min_lod = 0.0f;
    float max_lod = 0.0f;

    VkBorderColor border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    VkBool32 unnormalized_coordinates = VK_FALSE;

    Sampler() = default;
    Sampler(VulkanEngine* engine, const SamplerCreateDesc& desc);
    ~Sampler();

    void destroy();

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

    Sampler(Sampler&& other) noexcept;
    Sampler& operator=(Sampler&& other) noexcept;

    static SamplerCreateDesc linear_repeat_desc();
    static SamplerCreateDesc linear_clamp_desc();
    static SamplerCreateDesc nearest_repeat_desc();
    static SamplerCreateDesc cubemap_desc();
};
