#pragma once

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

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
#include "../renderer/aabb.h"
#include "path_planner.h"
#include "vehicle_command_sender.h"
#include "vehicle_geometry.h"
#include "waypoint_path.h"
#include "../utils/avg_timer.h"
#include "../a_star/vehicle.h"
#include "../a_star/local_planner_base.h"
#include "vehicle_state_receiver.h"

class VulkanQueue;
class ComputePassManager;
class VoxelGrid;
class ManagerBundle;
class VulkanSubmitContext;
class LocalPlanner;
class GazelleNext;

class Celeris {
public:
    _XCLASS_NAME(Celeris);

    using Waypoint = WaypointPath::Waypoint;

    struct CelerisDesc {
        uint16_t receiver_port = 5000;
        uint32_t voxel_point_map_num_hash_table_slots = 1500000;
        uint32_t voxel_point_map_max_map_point_count = 1500000;
        uint32_t max_write_count = 100000;
        uint32_t max_gicp_iterations = 10;
        bool lidar_accel_prediction_enabled = true;
        float lidar_accel_max_dt = 0.5f;
        float lidar_accel_max_mps2 = 20.0f;
        float lidar_velocity_max_mps = 25.0f;
        uint32_t unimpended_path_window_size = 64;
        uint32_t unimpended_path_max_astar_points = 4096;
        uint32_t collision_history_size = 8;
        uint32_t collision_escape_search_radius_voxels = 8;
        uint16_t vehicle_state_receiver_port = 5002;
        float vehicle_state_timeout = 0.25f;
        float max_vehicle_acceleration = 3;
        float max_vehicle_steer_acceleration = 5;
        float vehicle_wheel_base = 4.29f;
        float vehicle_cruise_speed = 20.0f;
        float vehicle_min_slowdown_acceleration = 2.0f;
        float vehicle_min_off_path_speed_factor = 0.25f;
        float vehicle_projection_backtrack_window = 0.75f;
        float vehicle_projection_lookahead_base = 3.0f;
        float vehicle_segment_switch_radius = 0.35f;
        float vehicle_min_direction_segment_virtual_length = 3.0f;
        float vehicle_direction_switch_arrival_speed = 0.9f;
        float vehicle_direction_switch_approach_speed = 0.75f;
        float global_path_direction_cleanup_min_segment_length = 1.0f;
        float local_planner_update_period = 0.05f;
        VehicleGeometry vehicle_geometry;
        uint32_t footprint_sample_count = 5;
        uint32_t footprint_horizontal_inflation_size = 1;
        uint32_t footprint_vertical_inflation_size = 1;
        float localization_probe_step = 4.0f;
        uint32_t localization_max_candidates = 512;
        uint32_t localization_gicp_iterations = 2;
        float waypoint_reach_radius = 2.0f;
        bool gamepad_commands_enabled = false;
        VehicleCommand gamepad_command;
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
        GazelleNext& gazelle,
        const CelerisDesc& desc
    );
            
    void start_lidar_receiver();
    void start(VulkanSubmitContext&& planner_submit_context);
    void update(VulkanSubmitContext& submit_context);

    void set_start(const NonholonomicPos& position);
    void set_goal(const NonholonomicPos& position);
    void set_start_lidar_scan_position(glm::vec3 position) noexcept;
    void set_start_lidar_scan_position(const NonholonomicPos& position) noexcept;
    void set_car_speed(float speed) noexcept;
    void set_waypoint_reach_radius(float radius) noexcept;
    void set_gamepad_commands_enabled(bool enabled) noexcept;
    void set_gamepad_command(VehicleCommand command) noexcept;
    void add_waypoint(glm::vec3 position);
    void add_waypoint(const NonholonomicPos& position);
    void delete_last_waypoint();

    const Transform& vehicle_transform() const noexcept;
    Transform vehicle_lidar_transform() const noexcept;
    LidarScan* network_scan();
    bool has_start_position() const noexcept;
    bool has_goal_position() const noexcept;
    NonholonomicPos start_position() const noexcept;
    NonholonomicPos goal_position() const noexcept;
    float car_speed() const noexcept;
    float vehicle_speed() const noexcept;
    float vehicle_steering_angle() const noexcept;
    const std::vector<Vehicle::SimulationControlCandidate>& local_planner_candidates() const noexcept;
    float local_planner_path_window_min_s() const noexcept;
    float local_planner_path_window_max_s() const noexcept;
    float local_planner_segment_switch_radius() const noexcept;
    float waypoint_reach_radius() const noexcept;
    bool gamepad_commands_enabled() const noexcept;
    VehicleCommand gamepad_command() const noexcept;
    bool command_sender_running() const noexcept;
    bool command_sender_connected() const noexcept;
    size_t active_waypoint_index() const noexcept;
    bool waypoint_path_completed() const noexcept;
    uint32_t received_scan_count() const noexcept;
    const std::vector<Waypoint>& waypoints() const noexcept;

    VulkanEngine* engine();

    GICPPass& gicp_pass();
    VoxelPointMap& voxel_point_map();
    VoxelMapPointInserter& voxel_map_point_inserter();
    VoxelMapPointReseter& voxel_map_reseter();
    Footprint& footprint() noexcept;
    VoxelGrid* voxel_grid() noexcept;
    WaypointPath& waypoint_path() noexcept;
    const WaypointPath& waypoint_path() const noexcept;

    bool request_path_replan();
    void reset_local_planner_tracking();
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
    // void visualize_active_path_potential();
    void sync_point_map_and_voxel_grid();
    void save_map(const std::filesystem::path& path);
    void load_map(const std::filesystem::path& path);
    void save_waypoint_path(const std::filesystem::path& path);
    void load_waypoint_path(const std::filesystem::path& path);
    // bool localize_on_map();
    const AABB& get_bounding_box() const noexcept;
    const AABB& get_bouding_box() const noexcept { return get_bounding_box(); }
    bool has_map_bounding_box() const noexcept;
    void set_vehicle_command(const VehicleCommand& vehicle_command);

private:
    VulkanEngine* m_engine = nullptr;
    ManagerBundle* m_manager_bundle = nullptr;
    MaterialInstanceManager* m_material_instance_manager = nullptr;
    VoxelGrid* m_voxel_grid = nullptr;
    Voxelizator* m_voxelizator = nullptr;
    VulkanBuffer* m_scan_vertex_buffer = nullptr;
    VulkanBuffer* m_scan_index_buffer = nullptr;
    PointCloudMesher* m_mesher = nullptr;
    GazelleNext* m_gazelle = nullptr;
    CelerisDesc m_desc;

    WaypointPath m_waypoint_path;
    GICPPass m_gicp_pass;
    PathIntersectionDetector m_path_intersection_detector;

    PointCloudPreprocessor m_point_cloud_preprocessor;
    LidarScanReceiver m_scan_receiver;
    VehicleCommandSender m_command_sender;

    VehicleStateReceiver m_vehicle_state_receiver;

    std::unique_ptr<VehicleBase> m_vehicle;
    PathPlanner m_path_planner;
    std::unique_ptr<LocalPlannerBase> m_local_planner;
    
    VoxelPointMap m_voxel_point_map;
    VoxelMapPointInserter m_voxel_map_inserter;
    VoxelMapPointReseter m_voxel_map_reseter;

    VulkanBuffer voxel_write_list;
    std::vector<glm::ivec3> m_path_potential_visualization_voxels;
    float m_path_potential_visualization_radius = 100.0f;
    float m_path_potential_visualization_step = 1.0f;
    float m_path_potential_visualization_vertical_drop = 10.0f;
    size_t m_last_path_potential_visualization_voxel_count = 0;
    AABB m_map_bounding_box{glm::vec4(0.0f), glm::vec4(0.0f)};
    bool m_has_map_bounding_box = false;
    bool m_needs_map_localization = false;

    NonholonomicPos m_start_position;
    // NonholonomicPos m_vehicle_position;
    NonholonomicPos m_goal_position;
    bool m_has_start_position = false;
    bool m_has_goal_position = false;
    Transform m_vehicle_transform;
    float m_car_speed = 10.0f;
    float m_waypoint_reach_radius = 2.0f;
    bool m_gamepad_commands_enabled = false;
    VehicleCommand m_gamepad_command;
    size_t m_active_waypoint_index = 0;
    bool m_waypoint_path_completed = false;

    std::unique_ptr<LidarScan> m_network_scan;
    std::deque<std::unique_ptr<LidarScan>> m_retired_network_scans;
    uint32_t m_received_scan_count = 0;
    std::chrono::steady_clock::time_point m_last_local_planner_update_timestamp{};
    bool m_has_last_local_planner_update_timestamp = false;
    bool m_has_previous_lidar_pose = false;
    bool m_has_start_lidar_scan_position = false;
    bool m_has_start_lidar_scan_rotation = false;
    glm::vec3 m_start_lidar_scan_position{0.0f};
    glm::quat m_start_lidar_scan_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_previous_lidar_position{0.0f};
    glm::quat m_previous_lidar_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 m_lidar_velocity{0.0f};
    glm::vec3 m_lidar_gravity{0.0f};
    bool m_has_lidar_gravity = false;
    bool m_has_previous_corrected_lidar_pose = false;
    glm::vec3 m_previous_corrected_lidar_position{0.0f};
    glm::quat m_previous_corrected_lidar_rotation{1.0f, 0.0f, 0.0f, 0.0f};
    uint64_t m_previous_corrected_lidar_timestamp_ns = 0;
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
    // void sync_vehicle_position_from_state(float height);
    VehicleBase& vehicle() noexcept;
    const VehicleBase& vehicle() const noexcept;
    LocalPlannerBase& local_planner() noexcept;
    const LocalPlannerBase& local_planner() const noexcept;
    Vehicle& mpc_vehicle() noexcept;
    const Vehicle& mpc_vehicle() const noexcept;
    LocalPlanner& mpc_local_planner() noexcept;
    const LocalPlanner& mpc_local_planner() const noexcept;

    bool is_path_impended(VulkanSubmitContext& submit_context);
    void reset_waypoint_navigation() noexcept;
    void update_waypoint_navigation();
    bool has_active_waypoint() const noexcept;
    NonholonomicPos waypoint_goal_pose(size_t waypoint_index) const;
    bool active_waypoint_goal_pose(NonholonomicPos& output) const;
    bool request_grounded_path_replan(NonholonomicPos start, NonholonomicPos goal);

    void sync_path_planner_result();
    Transform rear_axle_transform_from_lidar_transform(const Transform& lidar_transform) const;
    glm::vec3 rear_axle_midpoint_offset() const;
    glm::vec3 lidar_offset() const;

    bool find_closest_next_path_point(uint32_t current_id, uint32_t& output_id, uint32_t& output_dist);
    void update_map_bounding_box();
};
