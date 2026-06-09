#include "texture2d.h"
#include "../command_buffer.h"

Texture2D::Texture2D(
    VulkanEngine* engine,
    uint32_t width,
    uint32_t height,
    uint32_t mip_levels,
    VkImageUsageFlags usage,
    VkFormat format)
    :   engine(engine)
{
    if (engine == nullptr) {
        std::string message = "Texture2D::Texture2D: engine was nullptr";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (width == 0 || height == 0) {
        std::string message = "Texture2D::Texture2D: width and height must be greater than 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (mip_levels == 0) {
        std::string message = "Texture2D::Texture2D: mip_levels must be greater than 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    VkImageUsageFlags final_usage = usage;

    // We always need transfer dst for upload_data().
    final_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    // Needed if we want to generate mipmaps with vkCmdBlitImage.
    if (mip_levels > 1)
        final_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    image_resource = ImageResource(engine, ImageResource::texture2d_desc(format, width, height, mip_levels, final_usage));
    image_view = VulkanImageView(engine, VulkanImageView::texture2d_desc(&image_resource));

    Sampler::SamplerCreateDesc sampler_desc = Sampler::linear_repeat_desc();
    sampler_desc.max_lod = float(mip_levels - 1);

    sampler = Sampler(engine, sampler_desc);
}

void Texture2D::generate_mipmaps(CommandBuffer& command_buffer) {
    if (!engine) {
        std::string message = "Texture2D::generate_mipmaps: engine was nullptr";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    image_resource.generate_mipmaps(*engine, command_buffer);
}

void Texture2D::record_full_upload(
    CommandBuffer& command_buffer,
    VideoBuffer& staging_buffer)
{
    image_resource.transition_image_all(
        command_buffer,
        ImageTransitionState::ImageUsage::TransferDst,
        true
    );

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = image_resource.aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = image_resource.extent;

    vkCmdCopyBufferToImage(
        command_buffer.buffer,
        staging_buffer.buffer,
        image_resource.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    if (image_resource.mip_levels > 1) {
        generate_mipmaps(command_buffer);
    } else {
        image_resource.transition_image_all(
            command_buffer,
            ImageTransitionState::ImageUsage::SampledRead
        );
    }
}

void Texture2D::upload_data(
    CommandBuffer& command_buffer,
    VideoBuffer& staging,
    Fence& fence,
    const void* data,
    size_t size_bytes)
{
    if (!data) {
        std::string message = "Texture2D::upload_data: data == nullptr";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (size_bytes == 0) {
        std::string message = "Texture2D::upload_data: size_bytes == 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (size_bytes > staging.size_bytes) {
        std::string message = "Texture2D::upload_data: staging buffer too small";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    fence.wait_for_fence();
    fence.reset();

    staging.update_data(data, size_bytes);

    command_buffer.begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    record_full_upload(command_buffer, staging);
    command_buffer.end();
    command_buffer.submit(&fence);
}

void Texture2D::read_data(CommandBuffer& command_buffer, VideoBuffer& staging_buffer) {
    image_resource.transition_image_mip(command_buffer, 0, ImageTransitionState::ImageUsage::TransferSrc);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;     // tightly packed
    region.bufferImageHeight = 0;   // tightly packed
    region.imageSubresource.aspectMask = image_resource.aspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = image_resource.extent;

    vkCmdCopyImageToBuffer(
        command_buffer.buffer,
        image_resource.image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        staging_buffer.buffer,
        1,
        &region
    );

    image_resource.transition_image_mip(command_buffer, 0, ImageTransitionState::ImageUsage::SampledRead);
}

uint32_t Texture2D::calc_mip_levels(uint32_t width, uint32_t height) {
    return ImageResource::calc_mip_levels(width, height);
}
