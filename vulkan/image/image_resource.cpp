#include "image_resource.h"
#include "../command_buffer.h"
#include "../../vulkan_engine.h"
#include "../vulkan_utils.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

ImageResource::ImageResource(VulkanEngine* engine, const ImageCreateDesc& desc)
    :   engine(engine),
        image_type(desc.image_type),
        format(desc.format),
        extent(desc.extent),
        mip_levels(desc.mip_levels),
        array_layers(desc.array_layers),
        usage(desc.usage),
        flags(desc.flags),
        samples(desc.samples),
        tiling(desc.tiling),
        aspect(desc.aspect)
{
    transition_state = ImageTransitionState(desc.mip_levels, desc.array_layers);

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.flags = desc.flags;
    image_info.imageType = desc.image_type;
    image_info.format = desc.format;
    image_info.extent = desc.extent;
    image_info.mipLevels = desc.mip_levels;
    image_info.arrayLayers = desc.array_layers;
    image_info.samples = desc.samples;
    image_info.tiling = desc.tiling;
    image_info.usage = desc.usage;
    image_info.sharingMode = desc.sharing_mode;
    image_info.initialLayout = desc.initial_layout;

    if (desc.sharing_mode == VK_SHARING_MODE_CONCURRENT) {
        image_info.queueFamilyIndexCount =
            static_cast<uint32_t>(desc.queue_family_indices.size());
        image_info.pQueueFamilyIndices = desc.queue_family_indices.data();
    }

    vulkan_utils::vk_check(
        vkCreateImage(engine->device, &image_info, nullptr, &image),
        "vkCreateImage(ImageResource)"
    );

    VkMemoryRequirements mem_requirements{};
    vkGetImageMemoryRequirements(engine->device, image, &mem_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = vulkan_utils::find_memory_type(
        engine->physicalDevice,
        mem_requirements.memoryTypeBits,
        desc.memory_properties
    );

    vulkan_utils::vk_check(
        vkAllocateMemory(engine->device, &alloc_info, nullptr, &memory),
        "vkAllocateMemory(ImageResource)"
    );

    vulkan_utils::vk_check(
        vkBindImageMemory(engine->device, image, memory, 0),
        "vkBindImageMemory(ImageResource)"
    );
}

ImageResource::~ImageResource() {
    destroy();
}

void ImageResource::destroy() {
    if (image != VK_NULL_HANDLE || memory != VK_NULL_HANDLE)
    if (engine == nullptr || engine->device == VK_NULL_HANDLE) {
        std::string message = 
            "ImageResource::destroy: Memory leak due to inability to destroy object "
            "- missing engine pointer or engine->device not initialized.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (image != VK_NULL_HANDLE) {
        vkDestroyImage(engine->device, image, nullptr);
        image = VK_NULL_HANDLE;
    }

    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(engine->device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    engine = nullptr;
}

ImageResource::ImageResource(ImageResource&& other) noexcept
    :   engine(std::exchange(other.engine, nullptr)),
        transition_state(std::move(other.transition_state)),
        image(std::exchange(other.image, VK_NULL_HANDLE)),
        memory(std::exchange(other.memory, VK_NULL_HANDLE)),
        image_type(other.image_type),
        format(other.format),
        extent(other.extent),
        mip_levels(other.mip_levels),
        array_layers(other.array_layers),
        usage(other.usage),
        flags(other.flags),
        samples(other.samples),
        aspect(other.aspect),
        tiling(other.tiling)
{

}

ImageResource& ImageResource::operator=(ImageResource&& other) noexcept {
    if (this == &other)
        return *this;
    
    destroy();

    engine = std::exchange(other.engine, nullptr);
    transition_state = std::move(other.transition_state);
    image = std::exchange(other.image, VK_NULL_HANDLE);
    memory = std::exchange(other.memory, VK_NULL_HANDLE);

    image_type = other.image_type;
    format = other.format;
    extent = other.extent;
    mip_levels = other.mip_levels;
    array_layers = other.array_layers;
    usage = other.usage;
    flags = other.flags;
    samples = other.samples;
    aspect = other.aspect;
    tiling = other.tiling;

    return *this;
}

void ImageResource::transition_image(
    CommandBuffer& command_buffer,
    ImageTransitionState::ImageUsage new_usage,
    uint32_t base_mip_level,
    uint32_t level_count,
    uint32_t base_array_layer,
    uint32_t layer_count,
    bool discard_contents)
{
    if (command_buffer.buffer == VK_NULL_HANDLE) {
        std::string message = "ImageResource::transition_image: cmd was VK_NULL_HANDLE";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (image == VK_NULL_HANDLE) {
        std::string message = "ImageResource::transition_image: image was VK_NULL_HANDLE";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (level_count == 0 || layer_count == 0) {
        std::string message = "ImageResource::transition_image: level_count or layer_count is 0";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (base_mip_level + level_count > mip_levels) {
        std::string message = "ImageResource::transition_image: mip range out of bounds";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (base_array_layer + layer_count > array_layers) {
        std::string message = "ImageResource::transition_image: array layer range out of bounds";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    auto [it, inserted] = command_buffer.pending_image_states.try_emplace(this, transition_state);
    ImageTransitionState& local_state = it->second;
    
    for (uint32_t mip = base_mip_level; mip < base_mip_level + level_count; mip++) {
        for (uint32_t layer = base_array_layer; layer < base_array_layer + layer_count; layer++) {
            auto& state = local_state.subresource_state(mip, layer);

            if (!discard_contents && !state.known) {
                std::string message = "ImageResource::transition_layout: subresource layout is unknown";
                std::cout << message << std::endl;
                throw std::runtime_error(message);
            }

            ImageTransitionState::ImageUsage old_usage = discard_contents ? ImageTransitionState::ImageUsage::Undefined : state.usage;

            ImageTransitionState::UsageInfo old_usage_info = ImageTransitionState::get_usage_info(old_usage);
            ImageTransitionState::UsageInfo new_usage_info = ImageTransitionState::get_usage_info(new_usage);

            VkImageMemoryBarrier2 barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barrier.srcStageMask = discard_contents ? VK_PIPELINE_STAGE_2_NONE : old_usage_info.stage;
            barrier.srcAccessMask = discard_contents ? VK_ACCESS_2_NONE : old_usage_info.access;
            barrier.dstStageMask = new_usage_info.stage;
            barrier.dstAccessMask = new_usage_info.access;
            barrier.oldLayout = old_usage_info.layout;
            barrier.newLayout = new_usage_info.layout;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = image;
            barrier.subresourceRange.aspectMask = aspect;
            barrier.subresourceRange.baseMipLevel = mip;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = layer;
            barrier.subresourceRange.layerCount = 1;

            VkDependencyInfo dep{};
            dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep.imageMemoryBarrierCount = 1;
            dep.pImageMemoryBarriers = &barrier;

            vkCmdPipelineBarrier2(command_buffer.buffer, &dep);

            state.usage = new_usage;
            state.known = true;
        }
    }
}

void ImageResource::transition_image_all(
    CommandBuffer& command_buffer,
    ImageTransitionState::ImageUsage new_usage,
    bool discard_contents)
{
    transition_image(
        command_buffer,
        new_usage,
        0,
        mip_levels,
        0,
        array_layers,
        discard_contents
    );
}

void ImageResource::transition_image_mip(
    CommandBuffer& command_buffer,
    uint32_t mip_level,
    ImageTransitionState::ImageUsage new_usage,
    bool discard_contents)
{
    transition_image(
        command_buffer,
        new_usage,
        mip_level,
        1,
        0,
        array_layers,
        discard_contents
    );
}

void ImageResource::generate_mipmaps(CommandBuffer& command_buffer) {
    if (command_buffer.buffer == VK_NULL_HANDLE) {
        std::string message = "ImageResource::generate_mipmaps: cmd was VK_NULL_HANDLE";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (image == VK_NULL_HANDLE) {
        std::string message = "ImageResource::generate_mipmaps: image was VK_NULL_HANDLE";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (mip_levels <= 1)
        return;

    if ((usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0) {
        std::string message = "ImageResource::generate_mipmaps: image missing VK_IMAGE_USAGE_TRANSFER_SRC_BIT";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if ((usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
        std::string message = "ImageResource::generate_mipmaps: image missing VK_IMAGE_USAGE_TRANSFER_DST_BIT";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    VkFormatProperties format_properties{};
    vkGetPhysicalDeviceFormatProperties(engine.physicalDevice, format, &format_properties);

    if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) == 0) {
        std::string message = "ImageResource::generate_mipmaps: format does not support linear blitting";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) == 0) {
        std::string message = "ImageResource::generate_mipmaps: format missing VK_FORMAT_FEATURE_BLIT_SRC_BIT";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if ((format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT) == 0) {
        std::string message = "ImageResource::generate_mipmaps: format missing VK_FORMAT_FEATURE_BLIT_DST_BIT";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (samples != VK_SAMPLE_COUNT_1_BIT) {
        std::string message = "ImageResource::generate_mipmaps: vkCmdBlitImage requires VK_SAMPLE_COUNT_1_BIT";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (aspect != VK_IMAGE_ASPECT_COLOR_BIT) {
        std::string message = "generate_mipmaps: only color images are supported";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    int32_t mip_width = static_cast<int32_t>(extent.width);
    int32_t mip_height = static_cast<int32_t>(extent.height);

    for (uint32_t i = 1; i < mip_levels; ++i) {
        transition_image_mip(command_buffer, i - 1, ImageTransitionState::ImageUsage::TransferSrc);
        transition_image_mip(command_buffer, i, ImageTransitionState::ImageUsage::TransferDst, true);

        VkImageBlit blit{};
        blit.srcSubresource.aspectMask = aspect;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = array_layers;
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mip_width, mip_height, 1 };

        const int32_t next_width = (mip_width > 1) ? (mip_width / 2) : 1;
        const int32_t next_height = (mip_height > 1) ? (mip_height / 2) : 1;

        blit.dstSubresource.aspectMask = aspect;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = array_layers;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { next_width, next_height, 1 };

        vkCmdBlitImage(
            command_buffer.buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR
        );

        transition_image_mip(command_buffer, i - 1, ImageTransitionState::ImageUsage::SampledRead);

        mip_width = next_width;
        mip_height = next_height;
    }

    transition_image_mip(command_buffer, mip_levels - 1, ImageTransitionState::ImageUsage::SampledRead);
}

ImageResource::ImageCreateDesc ImageResource::texture2d_desc(
    VkFormat format,
    uint32_t width,
    uint32_t height,
    uint32_t mip_levels,
    VkImageUsageFlags usage
) {
    ImageCreateDesc desc{};
    desc.image_type = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.extent = { width, height, 1 };
    desc.mip_levels = mip_levels;
    desc.array_layers = 1;
    desc.usage = usage;
    desc.flags = 0;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    desc.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    return desc;
}

ImageResource::ImageCreateDesc ImageResource::cubemap_desc(
    VkFormat format,
    uint32_t face_size,
    uint32_t mip_levels,
    VkImageUsageFlags usage
) {
    ImageCreateDesc desc{};
    desc.image_type = VK_IMAGE_TYPE_2D;
    desc.format = format;
    desc.extent = { face_size, face_size, 1 };
    desc.mip_levels = mip_levels;
    desc.array_layers = 6;
    desc.usage = usage;
    desc.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.tiling = VK_IMAGE_TILING_OPTIMAL;
    desc.initial_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    desc.sharing_mode = VK_SHARING_MODE_EXCLUSIVE;
    desc.memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    desc.aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    return desc;
}

uint32_t ImageResource::calc_mip_levels(uint32_t width, uint32_t height) {
    return static_cast<uint32_t>(
        std::floor(std::log2(static_cast<float>(std::max(width, height))))
    ) + 1u;
}