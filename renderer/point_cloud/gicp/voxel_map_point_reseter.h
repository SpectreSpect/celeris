#pragma once

#include "../../../vulkan_self/vulkan_buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../../../vulkan_self/pass/instance/pass_instance.h"
#include "../../../vulkan_self/vulkan_fence.h"
#include "../../../vulkan_self/vulkan_command_buffer.h"

#include "../../../vulkan_self/logger/logger.h"

class ComputePassManager;
class VulkanEngine;
class VoxelPointMap;

class VoxelMapPointReseter {
public:
    _XCLASS_NAME(VoxelMapPointReseter);

    struct ReseterUniform {
        uint32_t num_hash_table_slots;
    };

    VoxelMapPointReseter(VulkanEngine& engine, ComputePassManager& compute_pass_manager);
    
    void reset(VoxelPointMap& voxel_point_map);
// reset_voxel_point_map_cp
private:
    VulkanEngine& engine;
    PassInstance reset_pass;
    VulkanBuffer uniform_buffer;

    VulkanCommandBuffer compute_command_buffer;
    VulkanFence compute_fence;
};