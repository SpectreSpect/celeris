#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

#include "../../vulkan_self/pass/instance/pass_instance.h"
#include "../../vulkan_self/vulkan_buffer.h"
#include "../../vulkan_self/vulkan_command_buffer.h"
#include "../../vulkan_self/vulkan_fence.h"
#include "../../vulkan_self/logger/logger.h"

class ComputePassManager;
class VulkanEngine;
class VulkanTexture2D;

class FlatColorTexturePass {
public:
    _XCLASS_NAME(FlatColorTexturePass);

    FlatColorTexturePass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    VulkanTexture2D generate(uint32_t width, uint32_t height, const glm::vec4& color);

private:
    struct alignas(16) UniformData {
        uint32_t image_width;
        uint32_t image_height;
        uint32_t padding[2];
        glm::vec4 color;
    };

    VulkanEngine& m_engine;
    VulkanBuffer m_uniform_buffer;
    PassInstance m_pass;
    VulkanCommandBuffer m_compute_command_buffer;
    VulkanFence m_compute_fence;
};
