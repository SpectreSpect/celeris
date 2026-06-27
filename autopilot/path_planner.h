#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

#include "../a_star/nonholonomic_a_star.h"
#include "../a_star/occupancy_grid_3d.h"
#include "../a_star/path_intersection_detector.h"
#include "../a_star/unimpended_path_finder.h"
#include "../utils/avg_timer.h"
#include "../vulkan_self/logger/logger_header.h"

class ComputePassManager;
class ManagerBundle;
class VulkanEngine;
class VulkanSubmitContext;
class VoxelGrid;

class PathPlanner {
public:
    _XCLASS_NAME(PathPlanner);

    struct PathPlannerDesc {
        uint32_t unimpended_path_window_size = 64;
        uint32_t unimpended_path_max_astar_points = 4096;
        NonholonomicAStar::NonholonomicAStarDesc nonholonomic_astar_desc;
    };

    struct PathPlannerResult {
        PlainAstarData plain_astar_path;
        std::vector<NonholonomicPos> nonholonomic_astar_path;
        std::vector<LineInstance> explored_paths;
        std::vector<NonholonomicPos> unimpended_path;
        AvgTimer total_path_finding_time;
        uint64_t generation = 0;
    };

    PathPlanner(
        VulkanEngine& engine,
        VulkanSubmitContext& submit_context,
        ManagerBundle& manager_bundle,
        VoxelGrid& voxel_grid,
        const PathPlannerDesc& desc
    );
    ~PathPlanner() noexcept;

    PathPlanner(const PathPlanner&) = delete;
    PathPlanner& operator=(const PathPlanner&) = delete;
    PathPlanner(PathPlanner&&) = delete;
    PathPlanner& operator=(PathPlanner&&) = delete;

    void start(VulkanSubmitContext&& submit_context);
    void stop() noexcept;

    void request_path_replan(NonholonomicPos start_pos, NonholonomicPos goal_pos);
    bool request_is_path_impended(VulkanSubmitContext& submit_context);

    bool request_adjust_to_ground(
        glm::vec3& output,
        int max_step_up = 500,
        int max_drop = 500,
        int max_y_diff = -1,
        bool allow_flying_over_precepices = true,
        uint32_t* status = nullptr
    );
    bool request_is_solid(glm::ivec3 voxel_pos);
    bool request_is_solid_world(glm::vec3 point);
    glm::vec3 request_voxel_size();
    glm::ivec3 request_world_to_voxel_pos(const glm::vec3& point);
    glm::vec3 request_voxel_to_world_pos(const glm::ivec3& voxel_pos);
    glm::vec3 request_voxel_center_world_pos(const glm::ivec3& voxel_pos);

    PathPlannerResult request_result_snapshot() const;
    uint64_t request_result_generation() const noexcept;
    bool request_has_path() const;

private:
    UnimpendedPathFinder m_unimpended_path_finder;
    PathIntersectionDetector m_path_intersection_detector;
    OccupancyGrid3D m_occupancy_grid;
    NonholonomicAStar m_planner;

    mutable std::mutex m_result_mutex;
    PathPlannerResult m_result;

    std::mutex m_planning_mutex;
    std::mutex m_occupancy_grid_mutex;
    std::mutex m_request_mutex;
    std::condition_variable m_request_cv;
    std::thread m_thread;
    bool m_running = false;
    bool m_replan_requested = false;
    NonholonomicPos m_requested_start;
    NonholonomicPos m_requested_goal;

    void planner_loop(VulkanSubmitContext submit_context);
    void plan_path(VulkanSubmitContext& submit_context, NonholonomicPos start_pos, NonholonomicPos goal_pos);
    std::vector<glm::vec3> current_path_points() const;
};
