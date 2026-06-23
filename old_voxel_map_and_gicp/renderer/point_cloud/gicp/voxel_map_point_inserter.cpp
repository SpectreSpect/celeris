#include "voxel_map_point_inserter.h"

#include "../../../vulkan_self/vulkan_engine.h"
#include "../../compute_pass_manager.h"
#include "../point_cloud.h"
#include "voxel_point_map.h"
#include "../point_instance.h"
#include "../point_cloud.h"

VoxelMapPointInserter::VoxelMapPointInserter(VulkanEngine& engine, ComputePassManager& compute_pass_manager) 
    :   engine(engine),
        insert_pass(compute_pass_manager.descriptor_pool(), compute_pass_manager.point_voxel_map_insert_cp),
        uniform_buffer(VulkanBuffer::create_host_visible_uniform_buffer(engine, sizeof(InserterUniform))),
        compute_command_buffer(engine.device(), engine.compute_command_pool()),
        compute_fence(engine.device()){

}

void VoxelMapPointInserter::insert(VoxelPointMap& voxel_point_map, PointCloud& source_point_cloud, VulkanBuffer& source_normal_buffer) {
    LOG_METHOD();
    // if (!this->engine)
    //     throw std::runtime_error("engine was null")

    logger.check(source_point_cloud.instance_buffer_view_valid(), "Source point cloud instance view was invalid");


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
    uniform_data.source_model = source_point_cloud.transform.get_model_matrix();
    // uniform_data.color = source_point_cloud.color;
    uniform_data.color = glm::vec4(1, 1, 1, 1);
    uniform_buffer.upload(&uniform_data, sizeof(InserterUniform));






    insert_pass.set_uniform_buffer(0, uniform_buffer);
    insert_pass.set_storage_buffer(1, *source_point_cloud.instance_buffer());
    insert_pass.set_storage_buffer(2, source_normal_buffer);
    insert_pass.set_storage_buffer(3, voxel_point_map.map_point_count_buffer);
    insert_pass.set_storage_buffer(4, voxel_point_map.map_point_buffer);
    insert_pass.set_storage_buffer(5, voxel_point_map.map_normal_buffer);
    insert_pass.set_storage_buffer(6, voxel_point_map.map_hash_table_buffer);

    // descriptor_set_bundle.bind_storage_buffer(1, source_point_cloud.instance_buffer);
    // descriptor_set_bundle.bind_storage_buffer(2, target_point_cloud.instance_buffer);

    // descriptor_set_bundle.bind_storage_buffer(3, source_normal_buffer);
    // descriptor_set_bundle.bind_storage_buffer(4, target_normal_buffer);

    // descriptor_set_bundle.bind_storage_buffer(5, output_buffer);
    
    // descriptor_set_bundle.bind_image_storage(1, brdf_lut_texture);

    uint32_t x_groups = div_up_u32(source_point_cloud.instance_count(), 256);

    {
        auto compute_scope = compute_command_buffer.begin_scope();
        
        insert_pass.bind(compute_command_buffer);

        compute_command_buffer.dispatch(x_groups, 1, 1);
    }

    compute_fence.reset();
    engine.compute_submit(compute_command_buffer, &compute_fence);
    compute_fence.wait();

    // voxel_point_map.map_point_count_buffer.read(&voxel_point_map.m_map_point_count, sizeof(uint32_t), 0);
    voxel_point_map.map_point_count_buffer.read(&voxel_point_map.m_map_point_count, sizeof(voxel_point_map.m_map_point_count), 0);
}