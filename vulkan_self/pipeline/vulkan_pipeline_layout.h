#pragma once

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <type_traits>
#include <cstdint>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "../logger/logger_header.h"
#include "../vulkan_command_buffer.h"
#include "../utils.h"

class VulkanDevice;
class DescriptorSetLayout;

struct PipelineLayoutBuilderDesc {
    VkDevice device = VK_NULL_HANDLE;
    std::vector<VkPushConstantRange> push_constant_ranges;
    std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
};

class PipelineLayoutBuilder {
public:
    _XCLASS_NAME(PipelineLayoutBuilder);
    
    PipelineLayoutBuilder() = default;

    PipelineLayoutBuilder& set_device(const VulkanDevice& device) noexcept;

    template<class T>
    static constexpr uint32_t push_constant_size_v() {
        static_assert(std::is_trivially_copyable_v<T>,
            "Push constant type must be trivially copyable");

        static_assert(sizeof(T) % 4 == 0,
            "Push constant size must be a multiple of 4 bytes");

        static_assert(sizeof(T) <= UINT32_MAX,
            "Push constant type is too large");

        return static_cast<uint32_t>(sizeof(T));
    }

    template<class T>
    PipelineLayoutBuilder& add_push_constants(
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT)
    {
        uint32_t next_offset = 0;

        for (const auto& range : m_desc.push_constant_ranges) {
            next_offset = std::max(
                next_offset,
                range.offset + range.size
            );
        }

        next_offset = Utils::align_up(next_offset, 4);

        return add_push_constants(
            push_constant_size_v<T>(),
            next_offset,
            stage_flags
        );
    }

    template<class T>
    inline PipelineLayoutBuilder& add_push_constants(
        uint32_t offset,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT)
    {
        return add_push_constants(push_constant_size_v<T>(), offset, stage_flags);
    }

    PipelineLayoutBuilder& add_push_constants(
        uint32_t size_bytes,
        uint32_t offset,
        VkShaderStageFlags stage_flags = VK_SHADER_STAGE_VERTEX_BIT
    );

    PipelineLayoutBuilder& add_descriptor_set_layout(const DescriptorSetLayout& layout);
    PipelineLayoutBuilder& prepend_descriptor_set_layout(const DescriptorSetLayout& layout);

    const PipelineLayoutBuilderDesc& desc() const noexcept;

private:
    PipelineLayoutBuilderDesc m_desc;
};

class VulkanPipelineLayout {
public:
    _XCLASS_NAME(VulkanPipelineLayout);

    explicit VulkanPipelineLayout(const VulkanDevice& device);
    explicit VulkanPipelineLayout(const PipelineLayoutBuilder& builder);
    ~VulkanPipelineLayout() noexcept;
    void destroy() noexcept;

    VulkanPipelineLayout(const VulkanPipelineLayout&) = delete;
    VulkanPipelineLayout& operator=(const VulkanPipelineLayout&) = delete;
    
    VulkanPipelineLayout(VulkanPipelineLayout&& other) noexcept;
    VulkanPipelineLayout& operator=(VulkanPipelineLayout&& other) noexcept;

    VkPipelineLayout handle() const noexcept;

    static PipelineLayoutBuilder create_builder() noexcept;

    template<class... Args>
    inline void push_constants(VulkanCommandBuffer& command_buffer, const Args&... args) const {
        LOG_METHOD();

        logger.check(sizeof...(Args) == push_constant_ranges.size())
            << "The number of push constants passed to the arguments does not match the number in the layout.";

        std::vector<std::pair<const void*, size_t>> arg_pointers;
        arg_pointers.reserve(sizeof...(Args));

        (arg_pointers.emplace_back(static_cast<const void*>(&args), sizeof(Args)), ...);

        for (size_t i = 0; i < push_constant_ranges.size(); i++) {
            logger.check(arg_pointers[i].second == push_constant_ranges[i].size)
                << "The size of push constant number " << std::to_string(i) << " does not match the layout\n";
            
            vkCmdPushConstants(
                command_buffer.handle(),
                m_pipeline_layout,
                push_constant_ranges[i].stageFlags,
                push_constant_ranges[i].offset,
                push_constant_ranges[i].size,
                arg_pointers[i].first
            );
        }
    }

private:
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    std::vector<VkPushConstantRange> push_constant_ranges;
};
