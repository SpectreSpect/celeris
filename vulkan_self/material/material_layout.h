#pragma once

#include "../logger/logger_header.h"
#include "../descriptor_set/descriptor_set_layout.h"
#include "../descriptor_set/descriptor_set_layout_builder.h"
#include "../vulkan_device.h"

class MaterialLayout {
public:
    _XCLASS_NAME(MaterialLayout);

    MaterialLayout() = delete;

    const DescriptorSetLayout& descriptor_set_layout() const noexcept {
        return ds_layout;
    }

    const DescriptorSetLayoutBuilder& descriptor_set_layout_builder() const noexcept {
        std::cout << dsl_builder.get_bindings().size() << std::endl;
        return dsl_builder;
    }

protected:
    MaterialLayout(const VulkanDevice& device, DescriptorSetLayoutBuilder builder)
        : dsl_builder(std::move(builder)),
          ds_layout(device, dsl_builder) {}

protected:
    DescriptorSetLayoutBuilder dsl_builder;
    DescriptorSetLayout ds_layout;
};