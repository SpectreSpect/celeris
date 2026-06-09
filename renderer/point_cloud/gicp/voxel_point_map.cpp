#include "voxel_point_map.h"

#include "../point_instance.h"
#include "../../../vulkan_self/vulkan_engine.h"
#include "../../instance_buffer_view.h"
#include "../../../voxel_grid_vulkan/voxel_grid_structures.h"
#include "../../../voxel_grid_vulkan/voxel_grid.h"
#include "../../../vulkan_self/utils.h"

VoxelPointMap::VoxelPointMap(VulkanEngine& engine, uint32_t num_hash_table_slots, uint32_t max_map_point_count)
    :   m_num_hash_table_slots(num_hash_table_slots),
        m_max_map_point_count(max_map_point_count),
        map_uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(VoxelPointMapUniform))),
        map_hash_table_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(VoxelHashSlotGpu) * num_hash_table_slots)),
        // map_point_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(PointInstance) * max_map_point_count)),
        map_point_buffer(VulkanBuffer(engine.physical_device(), engine.device(), sizeof(PointInstance) * max_map_point_count, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)),        
        map_normal_buffer(VulkanBuffer::create_storage_buffer(engine, sizeof(glm::vec4) * max_map_point_count)), 
        map_point_count_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t))){
}

InstanceBufferView VoxelPointMap::get_map_instance_view() {
    return InstanceBufferView(map_point_buffer, m_map_point_count, sizeof(PointInstance));
}

void VoxelPointMap::upload_voxels(VulkanEngine& engine, VoxelGrid& voxel_grid) {
    std::vector<PointInstance> points(m_map_point_count);

    map_point_buffer.read(points.data(), sizeof(PointInstance) * m_map_point_count, 0);

    std::vector<VoxelWriteGPU> voxel_writes;
    voxel_writes.reserve(m_map_point_count);

    for (int i = 0; i < points.size(); i++) {
        glm::ivec3 color{0, 98, 255};

        glm::vec3 local = glm::vec3(points[i].pos) / glm::vec3(voxel_grid.voxel_size());

        glm::ivec4 voxel_pos = glm::ivec4(glm::floor(local.x),
                                          glm::floor(local.y),
                                          glm::floor(local.z),
                                          1);

        voxel_writes.push_back(
            VoxelWriteGPU{
                .world_voxel = voxel_pos,
                .voxel_data = VoxelDataGPU(1, VOXEL_VISABILITY_FLAG_BIT, color),
                .set_flags = OVERWRITE_BIT
            }
        ); 
    }

    VulkanBuffer voxel_write_list = VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t) * 4 + Utils::size_bytes(voxel_writes));
    voxel_write_list.upload_scalar<uint32_t>(voxel_writes.size(), 0);
    voxel_write_list.upload(voxel_writes, sizeof(uint32_t) * 4);

    VulkanCommandBuffer compute_command_buffer(engine.device(), engine.compute_command_pool());
    {
        auto scope = compute_command_buffer.begin_scope();
        voxel_grid.set_voxels(compute_command_buffer, voxel_write_list);
    }
    VulkanFence compute_fence(engine.device());
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();
}