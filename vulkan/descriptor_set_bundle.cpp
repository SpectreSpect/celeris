#include "descriptor_set_bundle.h"
#include "image/texture2d.h"
#include "image/image_view.h"

void DescriptorSetBundle::bind_uniform_buffer(uint32_t binding, VideoBuffer& buffer) {
    descriptor_set.write_uniform_buffer(binding, buffer);
}

void DescriptorSetBundle::bind_combined_image_sampler(uint32_t binding, Texture2D& texture) {
    descriptor_set.write_combined_image_sampler(binding, texture);
}

void DescriptorSetBundle::bind_combined_image_sampler(uint32_t binding, Cubemap& texture) {
    descriptor_set.write_combined_image_sampler(binding, texture);
}

void DescriptorSetBundle::bind_storage_buffer(uint32_t binding, VideoBuffer& buffer) {
    descriptor_set.write_storage_buffer(binding, buffer);
}

void DescriptorSetBundle::bind_image_storage(uint32_t binding, Cubemap& texture) {
    descriptor_set.write_storage_image(binding, texture);
}

void DescriptorSetBundle::bind_image_storage(uint32_t binding, Texture2D& texture) {
    descriptor_set.write_storage_image(binding, texture);
}

void DescriptorSetBundle::bind_image_storage(uint32_t binding, VulkanImageView& image_view) {
    descriptor_set.write_storage_image(binding, image_view);
}

void DescriptorSetBundleBuilder::add_uniform_buffer(uint32_t binding, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage_flags, std::monostate{}};
}

void DescriptorSetBundleBuilder::add_storage_buffer(uint32_t binding, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage_flags, std::monostate{}};
}

void DescriptorSetBundleBuilder::add_combined_image_sampler(uint32_t binding, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage_flags, std::monostate{}};
}

void DescriptorSetBundleBuilder::add_image_storage(uint32_t binding, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage_flags, std::monostate{}};
}

void DescriptorSetBundleBuilder::add_uniform_buffer(uint32_t binding, VideoBuffer& video_buffer, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stage_flags, &video_buffer};
}

void DescriptorSetBundleBuilder::add_storage_buffer(uint32_t binding, VideoBuffer& video_buffer, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, stage_flags, &video_buffer};
}

void DescriptorSetBundleBuilder::add_combined_image_sampler(uint32_t binding, Texture2D& texture, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage_flags, &texture};
}

void DescriptorSetBundleBuilder::add_combined_image_sampler(uint32_t binding, Cubemap& texture, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stage_flags, &texture};
}

void DescriptorSetBundleBuilder::add_image_storage(uint32_t binding, Cubemap& texture, VkShaderStageFlags stage_flags) {
    entries[binding] = Entry{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, stage_flags, &texture};
}

DescriptorSetBundle DescriptorSetBundleBuilder::create(VkDevice& device) {
    DescriptorSetBundle bundle;

    if (entries.empty())
        throw std::runtime_error("DescriptorSetBundleBuilder doesn't have any entries. You have to add at least one binding.");

    for (auto it = entries.begin(); it != entries.end(); ++it) {
        switch (it->second.descriptor_type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                bundle.descriptor_set_layout.add_uniform_buffer(it->first, it->second.shader_stage_flags);
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
                bundle.descriptor_set_layout.add_storage_buffer(it->first, it->second.shader_stage_flags);
                break;

            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                bundle.descriptor_set_layout.add_combined_image_sampler(it->first, it->second.shader_stage_flags);
                break;

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
                bundle.descriptor_set_layout.add_image_storage(it->first, it->second.shader_stage_flags);
                break;

            default:
                throw std::runtime_error("Unknown descriptor type.");
        }
    }

    bundle.descriptor_set_layout.create(device);
    bundle.descriptor_pool.create(device, bundle.descriptor_set_layout);
    bundle.descriptor_set.create(device, bundle.descriptor_set_layout, bundle.descriptor_pool);

    for (auto it = entries.begin(); it != entries.end(); ++it) {
        switch (it->second.descriptor_type) {
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER: {
                if (auto ptr = std::get_if<VideoBuffer*>(&it->second.resource))
                    bundle.descriptor_set.write_uniform_buffer(it->first, **ptr);
                break;
            }

            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER: {
                if (auto ptr = std::get_if<VideoBuffer*>(&it->second.resource))
                    bundle.descriptor_set.write_storage_buffer(it->first, **ptr);
                break;
            }

            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: {
                if (auto ptr = std::get_if<Texture2D*>(&it->second.resource))
                    bundle.descriptor_set.write_combined_image_sampler(it->first, **ptr);
                else if (auto ptr = std::get_if<Cubemap*>(&it->second.resource))
                    bundle.descriptor_set.write_combined_image_sampler(it->first, **ptr);
                break;
            }

            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE: {
                if (auto ptr = std::get_if<Cubemap*>(&it->second.resource))
                    bundle.descriptor_set.write_storage_image(it->first, **ptr);
                break;
            }

            default:
                throw std::runtime_error("Unknown descriptor type.");
        }
    }

    return bundle;
}