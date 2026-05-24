#pragma once

#include "../../compute_pass_instance.h"
#include "../../../vulkan_self/vulkan_buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>


class ComputePassManager;
class VulkanEngine;

class GICPPass {
public:
    struct GICPReductorUniform {
        uint32_t input_count;
    };

    struct GICPPartial {
        double H[6][6];
        double g[6];
        double total_weighted_sq_error;
        uint32_t valid_count;
    };

    struct GICPPassUniform {
        glm::vec4 position;
        // glm::vec4 rotation;
        glm::vec4 rotation;
        uint32_t num_source_points;
        uint32_t num_target_points;
        uint32_t num_hash_table_slots;
        uint32_t _pad0;
    };

    static_assert(sizeof(GICPPassUniform) == 48);

    struct OutputBuffer {
        glm::vec4 position;
        glm::vec4 rotation;
    };


    GICPPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    // double step()();


    // ComputePassInstance test_instance(compute_pass_manager.descriptor_pool(), compute_pass_manager.test_compute_pass);
private:
    ComputePassInstance gicp_step_pass;

    // VulkanBuffer output_buffer;

    // GICPReductor reductor;

    VulkanBuffer uniform_buffer;
    VulkanBuffer output_buffer;

    VulkanBuffer partial_src;
    VulkanBuffer partial_dst;

    // VulkanBuffer rejection_buffer;

    uint32_t max_partial_count = 1000000;
};