#pragma once

#include "../managers/material_instance_manager.h"
#include "../managers/material_manager.h"
#include "../renderer/point_cloud/lidar/lidar_scan_receiver.h"
#include "../renderer/point_cloud/point_cloud_preprocessor.h"
#include "../renderer/point_cloud/gicp/voxel_point_map.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_reseter.h"
#include "../renderer/point_cloud/gicp/voxel_map_point_inserter.h"
#include "../renderer/point_cloud/point_cloud_mesher.h"
#include "../a_star/unimpended_path_finder.h"
#include "../a_star/nonholonomic_a_star.h"
#include "../a_star/occupancy_grid_3d.h"
#include "../renderer/point_cloud/gicp/gicp_pass.h"
#include "../voxel_grid_vulkan/voxelizator.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../vulkan_self/logger/logger_header.h"
#include "path_planner.h"
#include "vehicle_command_sender.h"
#include "vehicle_geometry.h"
#include "../utils/avg_timer.h"
#include "../a_star/vehichle.h"
#include "../a_star/local_planner.h"
#include "vehicle_state_receiver.h"

#include <chrono>
#include <cstddef>
#include <span>


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
        uint32_t collision_history_size = 8;
        uint32_t collision_escape_search_radius_voxels = 8;
        uint16_t vehicle_state_receiver_port = 5002;
        float vehicle_state_timeout = 0.25f;
        float max_vehicle_acceleration = 3;
        float max_vehicle_steer_acceleration = 2;
        float vehicle_wheel_base = 4.29f;
        float vehicle_cruise_speed = 10.0f;
        float vehicle_slowdown_distance_from_path = 2.5f;
        float vehicle_min_off_path_speed_factor = 0.25f;
        float vehicle_projection_backtrack_window = 0.75f;
        float vehicle_projection_lookahead_base = 3.0f;
        float vehicle_min_direction_segment_virtual_length = 3.0f;
        float vehicle_direction_switch_arrival_distance = 0.25f;
        float vehicle_direction_switch_arrival_speed = 0.9f;
        float vehicle_direction_switch_approach_speed = 0.75f;
        float global_path_direction_cleanup_min_segment_length = 1.0f;
        float local_planner_update_period = 0.05f;
        VehicleGeometry vehicle_geometry;
        uint32_t footprint_sample_count = 5;
        uint32_t footprint_horizontal_inflation_size = 1;
        uint32_t footprint_vertical_inflation_size = 1;
        NonholonomicAStar::NonholonomicAStarDesc nonholonomic_astar_desc;
    };

    Celeris(
        VulkanEngine& engine,
        VulkanQueue& compute_queue,
        VulkanSubmitContext& submit_context,
        ManagerBundle& manager_bundle,
        MaterialInstanceManager& material_instance_manager,
        VoxelGrid& voxel_grid,
        Voxelizator& voxelizator,
        VulkanBuffer& scan_vertex_buffer,
        VulkanBuffer& scan_index_buffer,
        PointCloudMesher& mesher,
        const CelerisDesc& desc
    );
            
    void start_lidar_receiver();
    void start(VulkanSubmitContext&& planner_submit_context);
    void update(VulkanSubmitContext& submit_context);

    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);
    void set_car_speed(float speed) noexcept;

    LidarScan* network_scan();
    const Transform& lidar_transform() const noexcept;
    bool has_start_position() const noexcept;
    bool has_goal_position() const noexcept;
    NonholonomicPos start_position() const noexcept;
    NonholonomicPos vehicle_position() const noexcept;
    NonholonomicPos goal_position() const noexcept;
    float car_speed() const noexcept;
    const std::vector<Vehicle::SimulationControlCandidate>& local_planner_candidates() const noexcept;
    float local_planner_path_window_min_s() const noexcept;
    float local_planner_path_window_max_s() const noexcept;
    uint32_t received_scan_count() const noexcept;

    VulkanEngine* engine();

    GICPPass& gicp_pass();
    VoxelPointMap& voxel_point_map();
    VoxelMapPointInserter& voxel_map_point_inserter();
    VoxelMapPointReseter& voxel_map_reseter();
    Footprint& footprint() noexcept;

    bool request_path_replan();
    bool adjust_to_ground(
        glm::vec3& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true
    );
    bool adjust_to_ground(
        std::vector<glm::vec3>& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true
    );
    bool get_ground_positions(
        const std::vector<glm::vec3>& polyline,
        std::vector<glm::ivec3>& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = false
    );
    bool has_planned_path() const;
    PathPlanner::PathPlannerResult path_result_snapshot() const;
    std::vector<NonholonomicPos> get_nonholonomic_astar_path() const;
    void display_path_planner_debug_controls();
    glm::vec3 voxel_size();
    glm::vec3 voxel_center_world_pos(const glm::ivec3& voxel_pos);

private:
    VulkanEngine* m_engine = nullptr;
    ManagerBundle* m_manager_bundle = nullptr;
    MaterialInstanceManager* m_material_instance_manager = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;
    Voxelizator* m_voxelizator = nullptr;
    VulkanBuffer* m_scan_vertex_buffer = nullptr;
    VulkanBuffer* m_scan_index_buffer = nullptr;
    PointCloudMesher* m_mesher = nullptr;
    CelerisDesc m_desc;

    GICPPass m_gicp_pass;
    PathIntersectionDetector m_path_intersection_detector;

    PointCloudPreprocessor m_point_cloud_preprocessor;
    LidarScanReceiver m_scan_receiver;
    VehicleCommandSender m_command_sender;

    VehicleStateReceiver m_vehicle_state_receiver;

    Vehicle m_vehicle;
    PathPlanner m_path_planner;
    LocalPlanner m_local_planner;
    
    VoxelPointMap m_voxel_point_map;
    VoxelMapPointInserter m_voxel_map_inserter;
    VoxelMapPointReseter m_voxel_map_reseter;

    VulkanBuffer voxel_write_list;

    NonholonomicPos m_start_position;
    NonholonomicPos m_vehicle_position;
    NonholonomicPos m_goal_position;
    bool m_has_start_position = false;
    bool m_has_goal_position = false;
    Transform m_lidar_transform;
    float m_car_speed = 10.0f;

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;
    uint32_t m_received_scan_count = 0;
    std::chrono::steady_clock::time_point m_last_local_planner_update_timestamp{};
    bool m_has_last_local_planner_update_timestamp = false;
    bool m_has_previous_lidar_pose = false;
    std::vector<glm::vec3> m_collision_raw_position_history;
    glm::vec3 m_collision_surface_point{0.0f};
    bool m_has_collision_surface_point = false;
    uint32_t path_replanning_interval = 5;

    uint64_t m_synced_path_generation = 0;
    uint64_t m_path_direction_cleanup_revision = 0;
    uint64_t m_synced_path_direction_cleanup_revision = 0;
    uint32_t current_target_path_point_id = 0;
    mutable std::mutex m_path_mutex;
    PlainAstarData plain_astar_path;
    std::vector<NonholonomicPos> nonholonomic_astar_path;
    std::vector<LineInstance> explored_paths;
    std::vector<NonholonomicPos> unimpended_path;
    AvgTimer total_path_finding_time;

    bool is_stop_waiting = false;
    double stop_waiting_time = 2;
    std::chrono::steady_clock::time_point stop_waiting_start_timestamp{};

    void collision(
        std::span<const glm::vec3> previous_free_raw_points,
        glm::vec3& point_pos
    );
    bool collision_point_is_free(glm::vec3 point);
    glm::vec3 collision_point_in_voxel_closest_to(glm::ivec3 voxel_pos, glm::vec3 reference);
    float collision_sample_step();
    bool find_first_free_collision_point_on_segment(glm::vec3 from, glm::vec3 to, glm::vec3& free_point);
    bool find_collision_surface_point(
        std::span<const glm::vec3> previous_free_raw_points,
        glm::vec3 point_pos,
        glm::vec3& surface_point
    );
    bool find_collision_escape_point(glm::vec3 point_pos, glm::vec3 direction, glm::vec3& resolved_pos);
    void remember_collision_raw_position(glm::vec3 point_pos);

    void apply_vehicle_feedback(const VehicleFeedback& feedback);
    bool is_vehicle_feedback_fresh(const VehicleFeedback& feedback) const;
    void sync_vehicle_position_from_state(float height);

    bool is_path_impended(VulkanSubmitContext& submit_context);

    void sync_path_planner_result();
    Transform rear_axle_transform_from_lidar_transform(const Transform& lidar_transform) const;
    Transform lidar_transform_from_rear_axle_transform(const Transform& rear_axle_transform) const;
    glm::vec3 rear_axle_midpoint_offset() const;
    glm::vec3 lidar_offset() const;

    bool find_closest_next_path_point(uint32_t current_id, uint32_t& output_id, uint32_t& output_dist);
};
