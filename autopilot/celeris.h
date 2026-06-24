#pragma once

#include "../renderer/point_cloud/lidar/lidar_scan_receiver.h"
#include "../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../renderer/point_cloud/gicp/voxel_point_map.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../a_star/unimpended_path_finder.h"
#include "../a_star/nonholonomic_a_star.h"
#include "../a_star/occupancy_grid_3d.h"
#include "../renderer/point_cloud/gicp/gicp_pass.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../vulkan_self/logger/logger_header.h"
#include "vehicle_command_sender.h"
#include "../utils/avg_timer.h"

#include <chrono>
#include <cstddef>


class VulkanQueue;
class ComputePassManager;
class VoxelGrid;
class ManagerBundle;
class VulkanSubmitContext;

class Celeris {
public:
    _XCLASS_NAME(Celeris);

    struct CelerisDesc {
        uint16_t receiver_port = 5000;
        uint32_t voxel_point_map_num_hash_table_slots = 1500000;
        uint32_t voxel_point_map_max_map_point_count = 1500000;
        uint32_t max_write_count = 100000;
        uint32_t max_gicp_iterations = 10;
        uint32_t unimpended_path_window_size = 64;
        uint32_t unimpended_path_max_astar_points = 4096;
        NonholonomicAStar::NonholonomicAStarDesc nonholonomic_astar_desc;
    };

    Celeris(
        VulkanEngine& engine,
        VulkanQueue& compute_queue,
        VulkanSubmitContext& submit_context,
        ManagerBundle& manager_bundle,
        VoxelGrid& voxel_grid,
        const CelerisDesc& desc
    );
            
    void start_lidar_receiver();
    void start(VulkanSubmitContext&& planner_submit_context);
    void update();

    void find_path(VulkanSubmitContext& submit_context);
    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);
    void set_car_speed(float speed) noexcept;

    LidarScan* network_scan();
    NonholonomicPos start_position() const noexcept;
    NonholonomicPos goal_position() const noexcept;
    float car_speed() const noexcept;
    NonholonomicAStar& planner();
    uint32_t received_scan_count() const noexcept;

    std::mutex& planner_mutex() noexcept;

    VulkanEngine* engine();

    GICPPass& gicp_pass();
    VoxelPointMap& voxel_point_map();
    VoxelMapPointInserter& voxel_map_point_inserter();
    VoxelMapPointReseter& voxel_map_reseter();

    PlainAstarData plain_astar_path;
    std::vector<NonholonomicPos> nonholonomic_astar_path;
    std::vector<LineInstance> explored_paths;
    std::vector<NonholonomicPos> unimpended_path;

    AvgTimer total_path_finding_time;

private:
    VulkanEngine* m_engine = nullptr;
    ManagerBundle* m_manager_bundle = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;
    CelerisDesc m_desc;

    GICPPass m_gicp_pass;

    PointCloudPreprocessor m_point_cloud_preprocessor;
    LidarScanReceiver m_scan_receiver;
    UnimpendedPathFinder m_unimpended_path_finder;
    VehicleCommandSender m_command_sender;
    OccupancyGrid3D m_occupancy_grid;
    NonholonomicAStar m_planner;
    
    VoxelPointMap m_voxel_point_map;
    VoxelMapPointInserter m_voxel_map_inserter;
    VoxelMapPointReseter m_voxel_map_reseter;

    VulkanBuffer voxel_write_list;

    NonholonomicPos m_start_position;
    NonholonomicPos m_goal_position;
    float m_car_speed = 10.0f;

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;
    uint32_t m_received_scan_count = 0;
    bool m_has_previous_lidar_pose = false;
    glm::vec3 m_previous_lidar_position{0.0f};
    glm::quat m_previous_lidar_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    uint32_t path_replanning_interval = 5;

    std::atomic<bool> m_planner_running{false};
    std::atomic<bool> m_replan_requested{false};
    std::mutex m_planner_mutex;
    std::thread m_planner_thread;

    uint32_t current_target_path_point_id = 0;

    bool is_stop_waiting = false;
    double stop_waiting_time = 2;
    std::chrono::steady_clock::time_point stop_waiting_start_timestamp{};
    
    void start_planner_thread(VulkanSubmitContext&& submit_context);
    void request_path_replan(const NonholonomicPos& start_pos, const NonholonomicPos& end_pos);
    void planner_loop(VulkanSubmitContext submit_context);

    bool find_closest_next_path_point(uint32_t current_id, uint32_t& output_id, uint32_t& output_dist);
    VehicleCommand get_path_following_command();
};
