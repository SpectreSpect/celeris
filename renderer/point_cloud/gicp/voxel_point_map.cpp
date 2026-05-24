#include "voxel_point_map.h"

#include "../point_instance.h"
#include "../../../vulkan_self/vulkan_engine.h"

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