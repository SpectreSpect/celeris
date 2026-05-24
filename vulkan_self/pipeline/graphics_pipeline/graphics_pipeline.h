#pragma once

#include <string>
#include <string_view>
#include <optional>
#include <cstdint>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include "../../logger/logger_header.h"
#include "../../vertex_layout_builder.h"
#include "../pipeline.h"

class VulkanShaderModule;
class VulkanPipelineLayout;
class VulkanRenderPass;
class VulkanDevice;
class VulkanCommandBuffer;
class DescriptorSetLayout;
class VulkanEngine;
class GraphicsPipelineBuilder;

class GraphicsPipeline : public Pipeline {
public:
    _XCHILD_NAME(GraphicsPipeline);

    explicit GraphicsPipeline(const GraphicsPipelineBuilder& builder);

    VkPipelineBindPoint get_bind_point() const noexcept override;

    static GraphicsPipelineBuilder create_builder() noexcept; // Микро функция для удобства

    static void set_y_down_viewport(
        VulkanCommandBuffer& command_buffer, 
        glm::vec2 size,
        glm::vec2 origin = glm::vec2{0.0f, 0.0f},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_down_viewport(
        VulkanCommandBuffer& command_buffer, 
        VkExtent2D size,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_up_viewport(
        VulkanCommandBuffer& command_buffer, 
        glm::vec2 size,
        glm::vec2 origin = glm::vec2{0.0f, 0.0f},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_up_viewport(
        VulkanCommandBuffer& command_buffer, 
        VkExtent2D size,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_scissor(
        VulkanCommandBuffer& command_buffer,
        VkExtent2D extent,
        VkOffset2D offset = VkOffset2D{0, 0}
    );

    static void set_y_down_viewport(
        VulkanCommandBuffer& command_buffer,
        VulkanEngine& engine,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_y_up_viewport(
        VulkanCommandBuffer& command_buffer,
        VulkanEngine& engine,
        VkOffset2D origin = VkOffset2D{0, 0},
        float min_depth = 0.0f,
        float max_depth = 1.0f
    );

    static void set_scissor(
        VulkanCommandBuffer& command_buffer,
        VulkanEngine& engine,
        VkOffset2D offset = VkOffset2D{0, 0}
    );

};
