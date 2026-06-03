#pragma once

#include "../../../vulkan_self/vulkan_buffer.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../../pass_instance.h"
#include "../../../vulkan_self/vulkan_fence.h"
#include "../../../vulkan_self/vulkan_command_buffer.h"

#include "../../../vulkan_self/logger/logger.h"

class ComputePassManager;
class VulkanEngine;
class VoxelPointMap;
class PointCloud;

class VoxelMapPointInserter {
public:
    _XCLASS_NAME(VoxelMapPointInserter);

    struct InserterUniform {
        uint32_t source_point_count;
        uint32_t max_map_point_count;
        uint32_t num_hash_table_slots;
        uint32_t _pad0;
        alignas(16) glm::mat4 source_model;
        glm::vec4 color;
    };

    VoxelMapPointInserter(VulkanEngine& engine, ComputePassManager& compute_pass_manager);
    
    void insert(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer);

    inline static uint32_t div_up_u32(uint32_t a, uint32_t b) { 
        return (a + b - 1u) / b; 
    }

private:
    VulkanEngine& engine;
    PassInstance insert_pass;
    VulkanBuffer uniform_buffer;

    VulkanCommandBuffer compute_command_buffer;
    VulkanFence compute_fence;
};