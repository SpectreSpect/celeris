#include "image_transition_state.h"

ImageTransitionState::ImageTransitionState(uint32_t mip_levels, uint32_t array_layers)
    :   mip_levels(mip_levels),
        array_layers(array_layers),
        subresource_states(static_cast<size_t>(mip_levels) * array_layers)
{
    
}

size_t ImageTransitionState::subresource_index(uint32_t mip, uint32_t layer) const {
    return (size_t)(layer) * mip_levels + mip;
}

ImageTransitionState::ImageSubresourceState& ImageTransitionState::subresource_state(uint32_t mip, uint32_t layer) {
    return subresource_states[subresource_index(mip, layer)];
}

const ImageTransitionState::ImageSubresourceState& ImageTransitionState::subresource_state(uint32_t mip, uint32_t layer) const {
    return subresource_states[subresource_index(mip, layer)];
}

void ImageTransitionState::set_subresource_state(uint32_t mip, uint32_t layer, ImageTransitionState::ImageUsage usage) {
    ImageSubresourceState& state = subresource_state(mip, layer);
    state.usage = usage;
    state.known = true;
}

ImageTransitionState::UsageInfo ImageTransitionState::get_usage_info(ImageUsage usage) {
    using U = ImageUsage;

    switch (usage) {
        case U::Undefined:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_NONE,
                .access = VK_ACCESS_2_NONE,
                .layout = VK_IMAGE_LAYOUT_UNDEFINED
            };

        case U::TransferDst:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_COPY_BIT,
                .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            };

        case U::TransferSrc:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_COPY_BIT,
                .access = VK_ACCESS_2_TRANSFER_READ_BIT,
                .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            };

        case U::SampledRead:
            return UsageInfo{
                // Консервативно: покрывает graphics + compute без хардкода на fragment-only.
                .stage  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .access = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                .layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
            };

        case U::ColorAttachment:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                .access = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT |
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
            };

        case U::DepthAttachment:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                          VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                .access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL
            };

        case U::StorageRead:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .access = VK_ACCESS_2_SHADER_STORAGE_READ_BIT,
                .layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
            };

        case U::StorageWrite:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                .access = VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                          VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_GENERAL
            };

        case U::Present:
            return UsageInfo{
                .stage  = VK_PIPELINE_STAGE_2_NONE,
                .access = VK_ACCESS_2_NONE,
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
            };
    }

    std::string message = "ImageResource::get_usage_info: unknown ImageUsage";
    std::cout << message << std::endl;
    throw std::runtime_error(message);
}
