#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../../../../vulkan_self/pass/instance/pass_instance.h"
#include "../../../../vulkan_self/vulkan_buffer.h"
#include "../../../../vulkan_self/vulkan_fence.h"
#include "../../../../vulkan_self/vulkan_command_buffer.h"
#include "gicp_reductor.h"

#include "../../../../vulkan_self/logger/logger.h"

class ComputePassManager;
class VulkanEngine;
class OldVoxelPointMap;
class OldPointCloud;
class VulkanBuffer;
class OldGICPReductor;


class OldGICPPass {
public:
    _XCLASS_NAME(OldGICPPass);

    struct GICPPassUniform {
        glm::vec4 position;
        // glm::vec4 rotation;
        glm::vec4 rotation;
        uint32_t num_source_points;
        uint32_t num_target_points;
        uint32_t num_hash_table_slots;
        uint32_t pack_bits;
        int32_t pack_offset;
        uint32_t _pad0[3];
    };

    static_assert(sizeof(GICPPassUniform) == 64);

    struct OutputBuffer {
        glm::vec4 position;
        glm::vec4 rotation;
    };


    OldGICPPass(VulkanEngine& engine, ComputePassManager& compute_pass_manager);

    double step(OldVoxelPointMap& voxel_point_map, OldPointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer);
    double fit(OldVoxelPointMap& voxel_point_map, OldPointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer, uint32_t max_steps);

private:
    uint32_t max_partial_count = 1000000;

    VulkanEngine& engine;

    PassInstance gicp_step_pass;
    VulkanCommandBuffer compute_command_buffer;
    VulkanFence compute_fence;

    // VulkanBuffer output_buffer;

    OldGICPReductor reductor;

    VulkanBuffer uniform_buffer;
    VulkanBuffer output_buffer;

    VulkanBuffer partial_src;
    VulkanBuffer partial_dst;

    VulkanBuffer rejection_buffer;

    static glm::quat omega_to_quat(const glm::vec3& omega);
    static glm::mat3 skew_matrix(const glm::vec3& v);
    static bool solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]);
    static glm::mat3 omega_to_mat3(const glm::vec3& omega);
};
