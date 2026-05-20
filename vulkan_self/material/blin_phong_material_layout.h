#pragma once

#include "material_layout.h"

class BlinPhongMaterialLayout : public MaterialLayout {
public:
    _XCHILD_NAME(BlinPhongMaterialLayout);

    explicit BlinPhongMaterialLayout(const VulkanDevice& device);

    static DescriptorSetLayoutBuilder create_builder();
};