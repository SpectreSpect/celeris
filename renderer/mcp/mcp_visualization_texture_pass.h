#pragma once

#include <cstdint>

#include <glm/glm.hpp>

#include "../../vulkan_self/logger/logger.h"
#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"

class ComputePassManager;
class VulkanEngine;
class VulkanTexture2D;

class McpVisualizationTexturePass {
public:
    _XCLASS_NAME(McpVisualizationTexturePass);

    McpVisualizationTexturePass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    VulkanTexture2D generate(
        uint32_t width,
        uint32_t height,
        const glm::mat4& quad_model = glm::mat4(1.0f),
        const glm::vec3& target_position = glm::vec3(0.0f),
        float distance_falloff = 1.0f
    );

    void render(
        VulkanTexture2D& texture,
        const glm::mat4& quad_model,
        const glm::vec3& target_position,
        float distance_falloff = 1.0f
    );

private:
    struct UniformData {
        glm::mat4 quad_model;
        glm::vec4 target_position_and_falloff;
    };

    VulkanEngine& m_engine;
    VulkanBuffer m_uniform_buffer;
    PassInstance m_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};
