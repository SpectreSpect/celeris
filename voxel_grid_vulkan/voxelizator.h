#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_command_pool.h"
#include "../vulkan_self/vulkan_fence.h"
#include "voxel_grid_structures.h"
#include "../vulkan_self/pass/instance/pass_instance.h"
#include "shader_helper/shader_helper.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanQueue;
class ComputePassManager;
class MeshView;

class Voxelizator {
public:
    _XCLASS_NAME(Voxelizator);

    struct VoxelizatorParams {
        glm::ivec3 chunk_size;
        glm::vec3 voxel_size;
        uint32_t counter_hash_table_size;
        uint32_t count_voxel_writes;
    };

    typedef VoxelizatorParams VoxelizatorDesc;

    explicit Voxelizator(
        const VulkanPhysicalDevice& physical_device,
        VulkanDevice& device,
        VulkanQueue& queue,
        ComputePassManager& compute_pass_manager,
        const VoxelizatorDesc& desc
    );
    ~Voxelizator() noexcept = default;

    Voxelizator(const Voxelizator&) = delete;
    Voxelizator& operator=(const Voxelizator&) = delete;

    Voxelizator(Voxelizator&&) noexcept = default;
    Voxelizator& operator=(Voxelizator&&) noexcept = default;

    void voxelize(
        VulkanCommandBuffer& command_buffer,
        const VoxelWriteGPU& prifab,
        MeshView mesh,
        uint32_t position_attribute_offset,
        uint32_t vertex_stride,
        glm::mat4 transform = glm::identity<glm::mat4>(),
        VulkanBuffer* out_voxel_writes = nullptr
    );

    void voxelize_and_submit(
        const VoxelWriteGPU& prifab,
        MeshView mesh,
        uint32_t position_attribute_offset,
        uint32_t vertex_stride,
        glm::mat4 transform = glm::identity<glm::mat4>(),
        VulkanBuffer* out_voxel_writes = nullptr
    );

private:
    struct VoxelizatorBuffers {
        VulkanBuffer dispatch_args;
        VulkanBuffer counter_hash_table;
        VulkanBuffer active_chunk_keys_list;
        VulkanBuffer triangle_indices_list;
        VulkanBuffer voxel_writes;
    };

    struct VoxelizatorPassInstances {
        PassInstance reset_voxelize_pipeline_pi;
        PassInstance mark_and_count_active_chunks_pi;
        PassInstance alloc_active_chunk_triangles_pi;
        PassInstance fill_triangle_indices_pi;
        PassInstance voxelize_triangles_pi;
    };

private:
    VulkanCommandPool m_command_pool;
    VulkanCommandBuffer m_command_buffer;
    VulkanFence m_fence;
    
    VulkanQueue* m_queue = nullptr;

    VoxelizatorParams m_params;
    VoxelizatorBuffers m_buffers;
    VoxelizatorPassInstances m_pass_instances;

    ShaderHelper m_shader_helper;

private:
    VoxelizatorBuffers create_buffers(
        const VulkanPhysicalDevice& physical_device,
        const VulkanDevice& device,
        VulkanCommandBuffer& command_buffer
    );

    VoxelizatorPassInstances create_pass_instances(VulkanDevice& device, ComputePassManager& compute_pass_manager) const;

    void submit_compute_commands();

    void reset_voxelize_pipline(VulkanCommandBuffer& command_buffer, VulkanBuffer& voxel_writes, bool reset_voxel_write_list = true);
    void mark_and_count_active_chunks(
        VulkanCommandBuffer& command_buffer,
        MeshView mesh,
        uint32_t position_attribute_offset,
        uint32_t vertex_stride,
        glm::mat4 transform = glm::identity<glm::mat4>()
    );
    void alloc_active_chunk_triangles(VulkanCommandBuffer& command_buffer, const VulkanBuffer& dispatch_args);
    void fill_triangle_indices(
        VulkanCommandBuffer& command_buffer,
        MeshView mesh,
        uint32_t position_attribute_offset,
        uint32_t vertex_stride,
        glm::mat4 transform = glm::identity<glm::mat4>()
    );
    void voxelize_chunks(
        VulkanCommandBuffer& command_buffer,
        const VulkanBuffer& dispatch_args,
        VulkanBuffer& voxel_writes,
        uint32_t voxel_type_flags,
        uint32_t voxel_color,
        uint32_t voxel_set_flags,
        MeshView mesh,
        uint32_t position_attribute_offset,
        uint32_t vertex_stride,
        glm::mat4 transform = glm::identity<glm::mat4>()
    );
};
