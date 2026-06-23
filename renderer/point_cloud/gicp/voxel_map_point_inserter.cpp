#include "voxel_map_point_inserter.h"

#include "../../../vulkan_self/vulkan_engine.h"
#include "../../../managers/compute_pass_manager.h"
#include "../point_cloud.h"
#include "voxel_point_map.h"
#include "../point_instance.h"
#include "../point_cloud.h"
#include "../../../math_utils.h"

#include <iostream>

VoxelMapPointInserter::VoxelMapPointInserter(VulkanEngine& engine, ComputePassManager& compute_pass_manager) 
    :   engine(engine),
        insert_pass(compute_pass_manager.point_voxel_map_insert_cp, compute_pass_manager.descriptor_pool()),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(InserterUniform))),
        hash_table_insert_fail_debug_buffer(VulkanBuffer::create_host_visible_storage_buffer(engine, sizeof(uint32_t))),
        compute_command_buffer(engine.device(), engine.compute_command_pool()),
        compute_fence(engine.device()){

}

void VoxelMapPointInserter::insert(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer) {
    LOG_METHOD();
    // if (!this->engine)
    //     throw std::runtime_error("engine was null")

    logger().check(source_point_cloud.instance_buffer_view_valid(), "Source point cloud instance view was invalid");


    // std::vector<PointInstance> points;
    // std::vector<glm::vec4> normals;

    // points.resize(15000);
    // normals.resize(15000);

    // for (int i = 0; i < points.size(); i++) {
    //     PointInstance point_instance;
    //     glm::vec4 normal = glm::vec4(0, 1, 0, 1);

    //     point_instance.pos = glm::vec4(i * 0.1f, 0, 0, 1);
    //     point_instance.color = glm::vec4(i * 0.1f, 0, 0, 1);

    //     points[i] = point_instance;
    //     normals[i] = normal;
    // }

    // source_point_cloud.instance_buffer()->upload(points.data(), points.size() * sizeof(PointInstance));
    // // source_point_cloud.set_instance_count(points.size());

    // source_normal_buffer.upload(normals.data(), normals.size() * sizeof(glm::vec4));
    
    InserterUniform uniform_data{};
    uniform_data.max_map_point_count = voxel_point_map.m_max_map_point_count;
    uniform_data.source_point_count = source_point_cloud.instance_count();
    // uniform_data.source_point_count = points.size();
    uniform_data.num_hash_table_slots = voxel_point_map.m_num_hash_table_slots;
    uniform_data.pack_bits = math_utils::BITS;
    uniform_data.pack_offset = static_cast<int32_t>(math_utils::OFFSET);
    uniform_data.retry_mode = 0u;
    uniform_data.source_model = source_point_cloud.transform.get_model_matrix();
    // uniform_data.color = source_point_cloud.color;
    uniform_data.color = glm::vec4(1, 1, 1, 1);

    uint32_t hash_table_insert_fail_count = 0;
    uint32_t retry_count = 0u;
    const uint32_t retry_attempt_count = 16u;
    const uint32_t retry_list_element_count = source_point_cloud.instance_count() + 1u;
    const VkDeviceSize retry_list_size = sizeof(uint32_t) * retry_list_element_count;

    VulkanBuffer retry_list_a = VulkanBuffer::create_host_visible_storage_buffer(engine, retry_list_size);
    VulkanBuffer retry_list_b = VulkanBuffer::create_host_visible_storage_buffer(engine, retry_list_size);

    auto dispatch_insert_pass = [&](uint32_t retry_mode, uint32_t invocation_count, VulkanBuffer& readable_retry_list, VulkanBuffer& writable_retry_list) {
        uniform_data.retry_mode = retry_mode;
        uniform_buffer.upload(&uniform_data, sizeof(InserterUniform));

        uint32_t zero = 0u;
        hash_table_insert_fail_debug_buffer.upload(&zero, sizeof(zero));
        writable_retry_list.upload(&zero, sizeof(zero));

        insert_pass.set_uniform_buffer(0, uniform_buffer);
        insert_pass.set_storage_buffer(1, source_point_cloud.instance_buffer());
        insert_pass.set_storage_buffer(2, source_normal_buffer);
        insert_pass.set_storage_buffer(3, voxel_point_map.map_point_count_buffer);
        insert_pass.set_storage_buffer(4, voxel_point_map.map_point_buffer);
        insert_pass.set_storage_buffer(5, voxel_point_map.map_normal_buffer);
        insert_pass.set_storage_buffer(6, voxel_point_map.map_hash_table_buffer);
        insert_pass.set_storage_buffer(7, hash_table_insert_fail_debug_buffer);
        insert_pass.set_storage_buffer(8, readable_retry_list);
        insert_pass.set_storage_buffer(9, writable_retry_list);

        uint32_t x_groups = div_up_u32(invocation_count, 256);

        {
            auto compute_scope = compute_command_buffer.begin_scope();
            
            insert_pass.bind(compute_command_buffer);

            compute_command_buffer.dispatch(x_groups, 1, 1);
        }

        compute_fence.reset();
        engine.compute_submit(compute_command_buffer, &compute_fence);
        compute_fence.wait();

        writable_retry_list.read(&retry_count, sizeof(retry_count), 0);
        hash_table_insert_fail_debug_buffer.read(&hash_table_insert_fail_count, sizeof(hash_table_insert_fail_count), 0);
    };

    dispatch_insert_pass(0u, source_point_cloud.instance_count(), retry_list_a, retry_list_b);
    std::cout << "Hash table insert failures: " << hash_table_insert_fail_count << std::endl;

    VulkanBuffer* readable_retry_list = &retry_list_b;
    VulkanBuffer* writable_retry_list = &retry_list_a;

    for (uint32_t retry_attempt = 0u; retry_count > 0u && retry_attempt < retry_attempt_count; retry_attempt++) {
        uint32_t current_retry_count = retry_count;

        dispatch_insert_pass(1u, current_retry_count, *readable_retry_list, *writable_retry_list);
        std::cout << "Hash table insert retry " << (retry_attempt + 1u)
                  << " failures: " << hash_table_insert_fail_count << std::endl;

        std::swap(readable_retry_list, writable_retry_list);
    }

    // voxel_point_map.map_point_count_buffer.read(&voxel_point_map.m_map_point_count, sizeof(uint32_t), 0);
    voxel_point_map.map_point_count_buffer.read(&voxel_point_map.m_map_point_count, sizeof(voxel_point_map.m_map_point_count), 0);

    if (retry_count > 0u) {
        std::cout << "Hash table insert failures remaining after retries: " << retry_count << std::endl;
    }
}
