#include "point_map_block.h"

#include "../../../managers/manager_bundle.h"
#include "../../sensors/lidar/lidar_scan.h"

PointMapBlock::PointMapBlock(
    VulkanEngine& engine,
    ManagerBundle& manager_bundle,
    const Desc& desc)
    :   m_desc(desc),
        m_voxel_point_map(
            engine,
            desc.hash_table_slots,
            desc.max_points),
        m_voxel_map_inserter(engine, manager_bundle.compute_pass_manager()),
        m_voxel_map_reseter(engine, manager_bundle.compute_pass_manager()),
        m_gicp_pass(engine, manager_bundle.compute_pass_manager()) {
    LOG_METHOD();
    logger().check(desc.hash_table_slots > 0,
                 "The number of voxel point map hash table slots must be greater than 0");
    logger().check(desc.max_points > 0,
                 "The maximum number of voxel point map points must be greater than 0");

    m_voxel_map_reseter.reset(m_voxel_point_map);
}

void PointMapBlock::fit(std::unique_ptr<LidarScan>& lidar_scan) {
    if (m_voxel_point_map.map_point_count() > 0u) {
        m_gicp_pass.fit(m_voxel_point_map,
                        lidar_scan->point_cloud(),
                        lidar_scan->normal_buffer(),
                        m_desc.max_gicp_iterations);
    }
}

void PointMapBlock::insert(std::unique_ptr<LidarScan>& lidar_scan) {
    m_voxel_map_inserter.insert(
        m_voxel_point_map, 
        lidar_scan->point_cloud(),
        lidar_scan->normal_buffer()
    );
}
