#pragma once

#include "../vulkan_engine.h"
#include "../vulkan/compute_pipeline.h"
#include "../vulkan/fence.h"
#include "../vulkan/image/texture2d.h"
#include "../vulkan/image/cubemap.h"
#include "../vulkan/graphics_pipeline.h"
#include "../vulkan/descriptor_set_bundle.h"
#include "../vulkan/render_target_2d.h"
#include "../vulkan/command_pool.h"
#include "../vulkan/command_buffer.h"
#include "../point_cloud/point_cloud.h"
#include "voxel_point_map.h"
#include "gicp_reductor.h"

class GICPPass {
public:
    // struct GICPPassUniform {
    //     glm::vec4 position;
    //     glm::vec4 rotation;
    //     uint32_t num_source_points;
    //     uint32_t num_target_points;
    //     uint32_t num_hash_table_slots;
    //     uint32_t _pad0;
    // };

    // static_assert(sizeof(GICPPassUniform) == 48);

    struct alignas(16) GICPPassUniform {
        glm::vec4 position;
        glm::vec4 rotation;

        uint32_t num_source_points;
        uint32_t num_target_points;
        uint32_t num_hash_table_slots;
        uint32_t _pad0;
    };

    static_assert(offsetof(GICPPassUniform, position) == 0);
    static_assert(offsetof(GICPPassUniform, rotation) == 16);
    static_assert(offsetof(GICPPassUniform, num_source_points) == 32);
    static_assert(sizeof(GICPPassUniform) == 48);

    struct OutputBuffer {
        glm::vec4 position;
        glm::vec4 rotation;
    };

    GICPPass() = default;

    void create(VulkanEngine& engine);
    double step(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer);
    double fit(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VideoBuffer& source_normal_buffer, uint32_t max_steps);

private:
    static glm::mat3 euler_xyz_to_mat3(const glm::vec3& euler);
    static glm::mat3 skew_matrix(const glm::vec3& v);
    static bool solve_6x6(const double H_in[6][6], const double g_in[6], double delta_out[6]);
    static glm::mat3 omega_to_mat3(const glm::vec3& omega);
    static glm::vec3 mat3_to_euler_xyz(const glm::mat3& R);

private:
    VulkanEngine* engine = nullptr;
    ComputePipeline pipeline;
    DescriptorSetBundle descriptor_set_bundle;
    Fence fence;
    VideoBuffer uniform_buffer;
    ShaderModule shader_module;

    uint32_t compute_queue_family_id = 0;
    VkQueue compute_queue = VK_NULL_HANDLE;

    CommandPool command_pool;
    CommandBuffer command_buffer;

    VideoBuffer output_buffer;

    GICPReductor reductor;

    VideoBuffer partial_src;
    VideoBuffer partial_dst;

    uint32_t max_partial_count = 1000000;
};