#include "image_view.h"
#include "../../vulkan_engine.h"

VulkanImageView::VulkanImageView(VulkanEngine* engine, const CreateDesc& desc) 
    :   engine(engine),
        view_type(desc.view_type),
        format(desc.format),
        subresource_range(desc.subresource_range),
        components(desc.components),
        flags(desc.flags)
{
    if (!desc.image) {
        std::string message = "ImageView::create: desc.image was nullptr";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (desc.image->image == VK_NULL_HANDLE) {
        std::string message = "ImageView::create: desc.image->image was VK_NULL_HANDLE";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    image = desc.image;

    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.flags = desc.flags;
    view_info.image = desc.image->image;
    view_info.viewType = desc.view_type;
    view_info.format = desc.format;
    view_info.components = desc.components;
    view_info.subresourceRange = desc.subresource_range;

    vulkan_utils::vk_check(
        vkCreateImageView(engine->device, &view_info, nullptr, &view),
        "vkCreateImageView(ImageView)"
    );
}

VulkanImageView::~VulkanImageView() {
    destroy();
}

void VulkanImageView::destroy() {
    if (view != VK_NULL_HANDLE)
    if (engine == nullptr || engine->device == VK_NULL_HANDLE) {
        std::string message = 
            "ImageView::destroy: Memory leak due to inability to destroy object "
            "- missing engine pointer or engine->device not initialized.";
        std::cout << message << std::endl;
        throw std::runtime_error(message);
    }

    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(engine->device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    engine = nullptr;
}

VulkanImageView::VulkanImageView(VulkanImageView&& other) noexcept
    :   engine(std::exchange(other.engine, nullptr)),
        image(std::exchange(other.image, nullptr)),
        view(std::exchange(other.view, VK_NULL_HANDLE)),
        view_type(std::exchange(other.view_type, VK_IMAGE_VIEW_TYPE_2D)),
        format(std::exchange(other.format, VK_FORMAT_UNDEFINED)),
        subresource_range(std::exchange(other.subresource_range, {})),
        components(std::exchange(other.components, {})),
        flags(std::exchange(other.flags, 0))

{

}

VulkanImageView& VulkanImageView::operator=(VulkanImageView&& other) noexcept {
    if (this == &other)
        return *this;
    
    destroy();

    engine = std::exchange(other.engine, nullptr);
    image = std::exchange(other.image, nullptr);
    view = std::exchange(other.view, VK_NULL_HANDLE);
    view_type = std::exchange(other.view_type, VK_IMAGE_VIEW_TYPE_2D);
    format = std::exchange(other.format, VK_FORMAT_UNDEFINED);
    subresource_range = std::exchange(other.subresource_range, {});
    components = std::exchange(other.components, {});
    flags = std::exchange(other.flags, 0);

    return *this;
}

VulkanImageView::CreateDesc VulkanImageView::texture2d_desc(ImageResource* image) {
    CreateDesc desc{};
    desc.image = image;
    desc.view_type = VK_IMAGE_VIEW_TYPE_2D;
    desc.format = image->format;
    desc.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    desc.subresource_range.aspectMask = image->aspect;
    desc.subresource_range.baseMipLevel = 0;
    desc.subresource_range.levelCount = image->mip_levels;
    desc.subresource_range.baseArrayLayer = 0;
    desc.subresource_range.layerCount = 1;
    desc.flags = 0;
    return desc;
}

VulkanImageView::CreateDesc VulkanImageView::cubemap_desc(ImageResource* image) {
    CreateDesc desc{};
    desc.image = image;
    desc.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    desc.format = image->format;
    desc.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    desc.subresource_range.aspectMask = image->aspect;
    desc.subresource_range.baseMipLevel = 0;
    desc.subresource_range.levelCount = image->mip_levels;
    desc.subresource_range.baseArrayLayer = 0;
    desc.subresource_range.layerCount = 6;
    desc.flags = 0;
    return desc;
}

VulkanImageView::CreateDesc VulkanImageView::cubemap_mip_desc(ImageResource* image, uint32_t mip_level) {
    if (!image)
        throw std::runtime_error("ImageView::cubemap_mip_desc: image was nullptr");

    if (image->array_layers < 6)
        throw std::runtime_error("ImageView::cubemap_mip_desc: image did not have 6 layers");

    if ((image->flags & VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) == 0)
        throw std::runtime_error("ImageView::cubemap_mip_desc: image was not cube compatible");

    if (mip_level >= image->mip_levels)
        throw std::runtime_error("ImageView::cubemap_mip_desc: mip_level was out of range");

    CreateDesc desc{};
    desc.image = image;
    desc.view_type = VK_IMAGE_VIEW_TYPE_CUBE;
    desc.format = image->format;
    desc.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    desc.subresource_range.aspectMask = image->aspect;
    desc.subresource_range.baseMipLevel = mip_level;
    desc.subresource_range.levelCount = 1;
    desc.subresource_range.baseArrayLayer = 0;
    desc.subresource_range.layerCount = 6;
    desc.flags = 0;
    return desc;
}
