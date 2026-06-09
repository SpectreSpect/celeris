#include "sampler.h"
#include "../vulkan_utils.h"

Sampler::Sampler(VulkanEngine* engine, const SamplerCreateDesc& desc) 
    :   engine(engine),
        mag_filter(desc.mag_filter),
        min_filter(desc.min_filter),
        mipmap_mode(desc.mipmap_mode),
        address_mode_u(desc.address_mode_u),
        address_mode_v(desc.address_mode_v),
        address_mode_w(desc.address_mode_w),
        mip_lod_bias(desc.mip_lod_bias),
        anisotropy_enable(desc.anisotropy_enable),
        max_anisotropy(desc.max_anisotropy),
        compare_enable(desc.compare_enable),
        compare_op(desc.compare_op),
        min_lod(desc.min_lod),
        max_lod(desc.max_lod),
        border_color(desc.border_color),
        unnormalized_coordinates(desc.unnormalized_coordinates)
{
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = desc.mag_filter;
    sampler_info.minFilter = desc.min_filter;
    sampler_info.mipmapMode = desc.mipmap_mode;

    sampler_info.addressModeU = desc.address_mode_u;
    sampler_info.addressModeV = desc.address_mode_v;
    sampler_info.addressModeW = desc.address_mode_w;

    sampler_info.mipLodBias = desc.mip_lod_bias;
    sampler_info.anisotropyEnable = desc.anisotropy_enable;
    sampler_info.maxAnisotropy = desc.max_anisotropy;

    sampler_info.compareEnable = desc.compare_enable;
    sampler_info.compareOp = desc.compare_op;

    sampler_info.minLod = desc.min_lod;
    sampler_info.maxLod = desc.max_lod;

    sampler_info.borderColor = desc.border_color;
    sampler_info.unnormalizedCoordinates = desc.unnormalized_coordinates;

    vulkan_utils::vk_check(
        vkCreateSampler(engine->device, &sampler_info, nullptr, &sampler),
        "vkCreateSampler(Sampler)"
    );
}

Sampler::~Sampler() {
    destroy();
}

void Sampler::destroy() {
    if (sampler != VK_NULL_HANDLE)
    if (engine == nullptr || engine->device == VK_NULL_HANDLE) {
        std::string message = 
            "Sampler::destroy: Memory leak due to inability to destroy object "
            "- missing engine pointer or engine->device not initialized.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(engine->device, sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }

    engine = nullptr;
}

Sampler::Sampler(Sampler&& other) noexcept 
    :   engine(std::exchange(other.engine, nullptr)),
        sampler(std::exchange(other.sampler, VK_NULL_HANDLE)),
        mag_filter(std::exchange(other.mag_filter, VK_FILTER_LINEAR)),
        min_filter(std::exchange(other.min_filter, VK_FILTER_LINEAR)),
        mipmap_mode(std::exchange(other.mipmap_mode, VK_SAMPLER_MIPMAP_MODE_LINEAR)),
        address_mode_u(std::exchange(other.address_mode_u, VK_SAMPLER_ADDRESS_MODE_REPEAT)),
        address_mode_v(std::exchange(other.address_mode_v, VK_SAMPLER_ADDRESS_MODE_REPEAT)),
        address_mode_w(std::exchange(other.address_mode_w, VK_SAMPLER_ADDRESS_MODE_REPEAT)),
        mip_lod_bias(std::exchange(other.mip_lod_bias, 0.0f)),
        anisotropy_enable(std::exchange(other.anisotropy_enable, VK_FALSE)),
        max_anisotropy(std::exchange(other.max_anisotropy, 1.0f)),
        compare_enable(std::exchange(other.compare_enable, VK_FALSE)),
        compare_op(std::exchange(other.compare_op, VK_COMPARE_OP_ALWAYS)),
        min_lod(std::exchange(other.min_lod, 0.0f)),
        max_lod(std::exchange(other.max_lod, 0.0f)),
        border_color(std::exchange(other.border_color, VK_BORDER_COLOR_INT_OPAQUE_BLACK)),
        unnormalized_coordinates(std::exchange(other.unnormalized_coordinates, VK_FALSE))
{

}

Sampler& Sampler::operator=(Sampler&& other) noexcept {
    if (this == &other)
        return *this;
    
    destroy();

    engine = std::exchange(other.engine, nullptr);
    sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
    mag_filter = std::exchange(other.mag_filter, VK_FILTER_LINEAR);
    min_filter = std::exchange(other.min_filter, VK_FILTER_LINEAR);
    mipmap_mode = std::exchange(other.mipmap_mode, VK_SAMPLER_MIPMAP_MODE_LINEAR);
    address_mode_u = std::exchange(other.address_mode_u, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    address_mode_v = std::exchange(other.address_mode_v, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    address_mode_w = std::exchange(other.address_mode_w, VK_SAMPLER_ADDRESS_MODE_REPEAT);
    mip_lod_bias = std::exchange(other.mip_lod_bias, 0.0f);
    anisotropy_enable = std::exchange(other.anisotropy_enable, VK_FALSE);
    max_anisotropy = std::exchange(other.max_anisotropy, 1.0f);
    compare_enable = std::exchange(other.compare_enable, VK_FALSE);
    compare_op = std::exchange(other.compare_op, VK_COMPARE_OP_ALWAYS);
    min_lod = std::exchange(other.min_lod, 0.0f);
    max_lod = std::exchange(other.max_lod, 0.0f);
    border_color = std::exchange(other.border_color, VK_BORDER_COLOR_INT_OPAQUE_BLACK);
    unnormalized_coordinates = std::exchange(other.unnormalized_coordinates, VK_FALSE);
}

Sampler::SamplerCreateDesc Sampler::linear_repeat_desc() {
    SamplerCreateDesc desc{};
    desc.mag_filter = VK_FILTER_LINEAR;
    desc.min_filter = VK_FILTER_LINEAR;
    desc.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    desc.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.mip_lod_bias = 0.0f;
    desc.anisotropy_enable = VK_FALSE;
    desc.max_anisotropy = 1.0f;
    desc.compare_enable = VK_FALSE;
    desc.compare_op = VK_COMPARE_OP_ALWAYS;
    desc.min_lod = 0.0f;
    desc.max_lod = 0.0f;
    desc.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    desc.unnormalized_coordinates = VK_FALSE;
    return desc;
}

Sampler::SamplerCreateDesc Sampler::linear_clamp_desc() {
    SamplerCreateDesc desc{};
    desc.mag_filter = VK_FILTER_LINEAR;
    desc.min_filter = VK_FILTER_LINEAR;
    desc.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    desc.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.mip_lod_bias = 0.0f;
    desc.anisotropy_enable = VK_FALSE;
    desc.max_anisotropy = 1.0f;
    desc.compare_enable = VK_FALSE;
    desc.compare_op = VK_COMPARE_OP_ALWAYS;
    desc.min_lod = 0.0f;
    desc.max_lod = 0.0f;
    desc.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    desc.unnormalized_coordinates = VK_FALSE;
    return desc;
}

Sampler::SamplerCreateDesc Sampler::nearest_repeat_desc() {
    SamplerCreateDesc desc{};
    desc.mag_filter = VK_FILTER_NEAREST;
    desc.min_filter = VK_FILTER_NEAREST;
    desc.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    desc.address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    desc.mip_lod_bias = 0.0f;
    desc.anisotropy_enable = VK_FALSE;
    desc.max_anisotropy = 1.0f;
    desc.compare_enable = VK_FALSE;
    desc.compare_op = VK_COMPARE_OP_ALWAYS;
    desc.min_lod = 0.0f;
    desc.max_lod = 0.0f;
    desc.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    desc.unnormalized_coordinates = VK_FALSE;
    return desc;
}

Sampler::SamplerCreateDesc Sampler::cubemap_desc() {
    SamplerCreateDesc desc{};
    desc.mag_filter = VK_FILTER_LINEAR;
    desc.min_filter = VK_FILTER_LINEAR;
    desc.mipmap_mode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    desc.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    desc.mip_lod_bias = 0.0f;
    desc.anisotropy_enable = VK_FALSE;
    desc.max_anisotropy = 1.0f;
    desc.compare_enable = VK_FALSE;
    desc.compare_op = VK_COMPARE_OP_ALWAYS;
    desc.min_lod = 0.0f;
    desc.max_lod = 0.0f;
    desc.border_color = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    desc.unnormalized_coordinates = VK_FALSE;
    return desc;
}
