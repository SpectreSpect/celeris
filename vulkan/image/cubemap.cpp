#include "cubemap.h"
#include "../command_buffer.h"

Cubemap::Cubemap(
    VulkanEngine* engine,
    uint32_t face_size,
    uint32_t mip_levels,
    VkImageUsageFlags usage,
    VkFormat format) 
{
    if (face_size == 0) {
        std::string message = "Cubemap::create: face size must be greater than 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (mip_levels == 0) {
        std::string message = "Cubemap::create: mip_levels must be greater than 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    this->engine = &engine;
    this->face_size = face_size;

    VkImageUsageFlags final_usage = usage;

    // Useful if you ever upload mip 0 / copy into the cubemap.
    final_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Needed for generic vkCmdBlitImage mip generation.
    if (mip_levels > 1)
        final_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    image_resource = ImageResource(engine, ImageResource::cubemap_desc(format, face_size, mip_levels, final_usage));
    image_view = VulkanImageView(engine, VulkanImageView::cubemap_desc(&image_resource));

    Sampler::SamplerCreateDesc sampler_desc = Sampler::cubemap_desc();
    sampler_desc.max_lod = float(mip_levels - 1);
    sampler = Sampler(engine, sampler_desc);
}

uint32_t Cubemap::calc_mip_levels(uint32_t face_size) {
    return ImageResource::calc_mip_levels(face_size, face_size);
}
