#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vulkan/vulkan.h>

class ImageTransitionState {
public:
    enum class ImageUsage {
        Undefined = 0,
        TransferDst = 1,
        TransferSrc = 2,
        SampledRead = 3,
        ColorAttachment = 4,
        DepthAttachment = 5,
        StorageRead = 6,
        StorageWrite = 7,
        Present = 8
    };

    struct UsageInfo {
        VkPipelineStageFlags2 stage;
        VkAccessFlags2 access;
        VkImageLayout layout;
    };

    struct ImageSubresourceState {
        ImageUsage usage = ImageUsage::Undefined;
        bool known = false;
    };

    uint32_t mip_levels;
    uint32_t array_layers;
    std::vector<ImageSubresourceState> subresource_states;

    ImageTransitionState() = default;
    ImageTransitionState(uint32_t mip_levels, uint32_t array_layers);

    ImageTransitionState(const ImageTransitionState&) = default;
    ImageTransitionState& operator=(const ImageTransitionState&) = default;
    
    ImageTransitionState(ImageTransitionState&&) noexcept = default;
    ImageTransitionState& operator=(ImageTransitionState&&) noexcept = default;

    size_t subresource_index(uint32_t mip, uint32_t layer) const;
    ImageSubresourceState& subresource_state(uint32_t mip, uint32_t layer);
    const ImageSubresourceState& subresource_state(uint32_t mip, uint32_t layer) const;
    void set_subresource_state(uint32_t mip, uint32_t layer, ImageUsage usage);

    static UsageInfo get_usage_info(ImageUsage usage);
};
