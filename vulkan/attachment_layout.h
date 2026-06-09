#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class AttachmentLayout {
public:
    std::vector<VkAttachmentDescription> attachment_descriptions;

    AttachmentLayout();
    void create();
    void add_color_attachment(VkFormat format);
    void add_depth_attachment(VkFormat format);
};