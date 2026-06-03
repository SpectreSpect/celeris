#include "voxel_map_point_reseter.h"

#include "../../compute_pass_manager.h"
#include "../../../vulkan_self/vulkan_engine.h"
#include "voxel_point_map.h"
#include "../../../math_utils.h"

VoxelMapPointReseter::VoxelMapPointReseter(VulkanEngine& engine, ComputePassManager& compute_pass_manager) 
    :   engine(engine),
        reset_pass(compute_pass_manager.reset_voxel_point_map_cp, compute_pass_manager.descriptor_pool()),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(ReseterUniform))),
        compute_command_buffer(engine.device(), engine.compute_command_pool()),
        compute_fence(engine.device()) {}

void VoxelMapPointReseter::reset(VoxelPointMap& voxel_point_map) {
    LOG_METHOD();

    ReseterUniform uniform_data{};
    uniform_data.num_hash_table_slots = voxel_point_map.m_num_hash_table_slots;
    uniform_buffer.upload(&uniform_data, sizeof(ReseterUniform));

    reset_pass.set_uniform_buffer(0, uniform_buffer);
    reset_pass.set_storage_buffer(2, voxel_point_map.map_point_count_buffer);
    reset_pass.set_storage_buffer(4, voxel_point_map.map_hash_table_buffer);

    uint32_t x_groups = math_utils::div_up_u32(uniform_data.num_hash_table_slots, 256);
    // uint32_t x_groups = uniform_data.num_hash_table_slots / 256;

    {
        auto compute_scope = compute_command_buffer.begin_scope();
        
        reset_pass.bind(compute_command_buffer);

        compute_command_buffer.dispatch(x_groups, 1, 1);
    }

    compute_fence.reset();
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();

    voxel_point_map.m_map_point_count = 0;
}