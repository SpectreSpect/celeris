#pragma once

#include "../renderer/point_cloud/lidar/lidar_scan_receiver.h"
#include "../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../renderer/point_cloud/gicp/voxel_point_map.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../a_star/nonholonomic_a_star.h"
#include "../a_star/occupancy_grid_3d.h"
#include "../renderer/point_cloud/gicp/gicp_pass.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/logger/logger_header.h"


class VulkanQueue;
class ComputePassManager;
class VoxelGrid;

class Celeris {
public:
    _XCLASS_NAME(Celeris);

    struct CelerisDesc {
        uint16_t receiver_port = 5000;
        uint32_t voxel_point_map_num_hash_table_slots = 1500000;
        uint32_t voxel_point_map_max_map_point_count = 1500000;
        uint32_t max_write_count = 100000;
        uint32_t max_gicp_iterations = 10;
        NonholonomicAStar::NonholonomicAStarDesc nonholonomic_astar_desc;
    };

    Celeris(VulkanEngine& engine, 
            VulkanQueue& compute_queue, 
            ManagerBundle& manager_bundle, 
            VoxelGrid& voxel_grid,
            CelerisDesc desc);
            
    void start_lidar_receiver();
    void start();
    void update();

    void find_path();
    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);

    LidarScan* network_scan();
    NonholonomicPos start_position() const noexcept;
    NonholonomicPos goal_position() const noexcept;
    NonholonomicAStar& planner();
    uint32_t received_scan_count() const noexcept;

    std::mutex& planner_mutex() noexcept;

    VulkanEngine* engine();

    PlainAstarData plain_astar_path;
    std::vector<NonholonomicPos> nonholonomic_astar_path;
    std::vector<LineInstance> explored_paths;
    std::vector<NonholonomicPos> unimpended_path;

private:
    VulkanEngine* m_engine = nullptr;
    ManagerBundle* m_manager_bundle = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;
    CelerisDesc m_desc;

    GICPPass m_gicp_pass;

    PointCloudPreprocessor m_point_cloud_preprocessor;
    LidarScanReceiver m_scan_receiver;
    OccupancyGrid3D m_occupancy_grid;
    NonholonomicAStar m_planner;
    
    VoxelPointMap m_voxel_point_map;
    VoxelMapPointInserter m_voxel_map_inserter;
    VoxelMapPointReseter m_voxel_map_reseter;

    VulkanBuffer voxel_write_list;

    NonholonomicPos m_start_position;
    NonholonomicPos m_goal_position;

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;
    uint32_t m_received_scan_count = 0;
    uint32_t path_replanning_interval = 5;

    std::atomic<bool> m_planner_running{false};
    std::atomic<bool> m_replan_requested{false};
    std::mutex m_planner_mutex;
    std::thread m_planner_thread;
    
    void start_planner_thread();
    void request_path_replan(const NonholonomicPos& start_pos, const NonholonomicPos& end_pos);
    void planner_loop();
};
