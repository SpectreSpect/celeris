#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <concepts>
#include <type_traits>

#include "../vulkan_self/logger/logger_header.h"
#include "../vulkan_self/vulkan_buffer.h"
#include "../vulkan_self/vulkan_command_buffer.h"
#include "../vulkan_self/vulkan_command_pool.h"
#include "../vulkan_self/vulkan_fence.h"
#include "voxel_grid_structures.h"
#include "../vulkan_self/pass/instance/pass_instance.h"
#include "shader_helper/shader_helper.h"
#include "../renderer/mesh_view.h"
#include "shader_helper/buffer_dispatch_arg.h"
#include "shader_helper/value_dispatch_arg.h"

class VulkanPhysicalDevice;
class VulkanDevice;
class VulkanQueue;
class ComputePassManager;

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

    template <class Vertex>
    void voxelize(
        VulkanCommandBuffer& command_buffer,
        const VoxelWriteGPU& prifab,
        MeshView mesh,
        glm::mat4 transform = glm::identity<glm::mat4>(),
        VulkanBuffer* out_voxel_writes = nullptr)
    {
        LOG_METHOD();

        static_assert(
            std::is_standard_layout_v<Vertex>,
            "Vertex must be a standard-layout type"
        );
        static_assert(
            std::is_trivially_copyable_v<Vertex>,
            "Vertex must be a trivially copyable type"
        );
        static_assert(
            requires(const Vertex& obj){{obj.position} -> std::same_as<const glm::vec4&>;}, 
            "Vertex must have glm::vec4 position"
        );

        constexpr uint32_t vertex_stride = sizeof(Vertex);
        constexpr uint32_t vertex_position_offset = offsetof(Vertex, position);

        VulkanBuffer* voxel_writes_buffer = out_voxel_writes ? out_voxel_writes : &m_buffers.voxel_writes;

        reset_voxelize_pipline(command_buffer, *voxel_writes_buffer, out_voxel_writes == nullptr);

        mark_and_count_active_chunks(command_buffer, mesh, vertex_position_offset, vertex_stride, transform);

        m_shader_helper.prepare_dispatch_args(
            command_buffer,
            m_buffers.dispatch_args,
            BufferDispatchArg(&m_buffers.active_chunk_keys_list, 0)
        );
        alloc_active_chunk_triangles(command_buffer, m_buffers.dispatch_args);

        fill_triangle_indices(command_buffer, mesh, vertex_position_offset, vertex_stride, transform);

        m_shader_helper.prepare_dispatch_args(
            command_buffer,
            m_buffers.dispatch_args, 
            ValueDispatchArg(m_params.chunk_size.x * m_params.chunk_size.y * m_params.chunk_size.z), 
            BufferDispatchArg(&m_buffers.active_chunk_keys_list, 0)
        );
        voxelize_chunks(
            command_buffer,
            m_buffers.dispatch_args,
            *voxel_writes_buffer,
            prifab.voxel_data.type_flags,
            prifab.voxel_data.color,
            prifab.set_flags,
            mesh,
            vertex_position_offset,
            vertex_stride,
            transform
        );

        if (out_voxel_writes == nullptr) {
            logger.throw_error("Not implemented yet :D");
            // if (gridable_gpu != nullptr) {
            //     gridable_gpu->set_voxels(voxel_writes_);
            // } else {
            //     //TODO
            // }
        }
    }


    template<class Vertex>
    void voxelize(
        const VoxelWriteGPU& prifab,
        MeshView mesh,
        glm::mat4 transform,
        VulkanBuffer* out_voxel_writes)
    {
        LOG_METHOD();

        {
            auto scope = m_command_buffer.begin_scope();
            voxelize<Vertex>(
                m_command_buffer,
                prifab,
                mesh,
                transform,
                out_voxel_writes
            );
        }

        submit_compute_commands();
    }

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
