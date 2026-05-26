#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../../compute_pass_instance.h"
#include "../../../vulkan_self/vulkan_buffer.h"
#include "../../../vulkan_self/vulkan_fence.h"
#include "../../../vulkan_self/vulkan_command_buffer.h"

#include "../../../vulkan_self/logger/logger.h"

class ComputePassManager;
class VulkanEngine;
class VulkanBuffer;

class GICPReductor {
public:
    _XCLASS_NAME(GICPReductor);

    struct GICPReductorUniform {
        uint32_t input_count;
    };

    struct GICPPartial {
        double H[6][6];
        double g[6];
        double total_weighted_sq_error;
        uint32_t valid_count;
    };


    GICPReductor(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    uint32_t reduce_step(VulkanBuffer& partial_src, VulkanBuffer& partial_dst, const uint32_t input_count);
    GICPPartial reduce(VulkanBuffer& partial_src, VulkanBuffer& partial_dst, const uint32_t input_count);

private:
    VulkanEngine& engine;

    ComputePassInstance gicp_reduce_pass;
    VulkanCommandBuffer compute_command_buffer;
    VulkanFence compute_fence;

    VulkanBuffer uniform_buffer;
};