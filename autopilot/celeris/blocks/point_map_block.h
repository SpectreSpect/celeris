#pragma once

#include <cstdint>

#include "../../../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../../../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../../../renderer/point_cloud/gicp/voxel_point_map.h"
#include "../../../renderer/point_cloud/gicp/gicp_pass.h"
#include "../../../vulkan_self/logger/logger_header.h"

class ManagerBundle;
class VulkanEngine;
class LidarScan;

class PointMapBlock {
public:
    _XCLASS_NAME(PointMapBlock);

    struct PointMapBlockDesc {
        uint32_t voxel_point_map_num_hash_table_slots = 1500000;
        uint32_t voxel_point_map_max_map_point_count = 1500000;
        uint32_t max_gicp_iterations = 10;
    };

    PointMapBlock(
        VulkanEngine& engine,
        ManagerBundle& manager_bundle,
        const PointMapBlockDesc& desc
    );

    void fit(std::unique_ptr<LidarScan>& lidar_scan);
    void insert(std::unique_ptr<LidarScan>& lidar_scan);

private:
    PointMapBlockDesc m_desc;

    VoxelPointMap m_voxel_point_map;
    VoxelMapPointInserter m_voxel_map_inserter;
    VoxelMapPointReseter m_voxel_map_reseter;
    GICPPass m_gicp_pass;
};
