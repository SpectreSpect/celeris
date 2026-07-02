#include "path_planner.h"

#include <iostream>
#include <utility>

#include "../managers/manager_bundle.h"
#include "../vulkan_self/vulkan_engine.h"
#include "../vulkan_self/vulkan_submit_context.h"
#include "../voxel_grid_vulkan/voxel_grid.h"

PathPlanner::PathPlanner(
    VulkanEngine& engine,
    VulkanSubmitContext& submit_context,
    ManagerBundle& manager_bundle,
    VoxelGrid& voxel_grid,
    PathIntersectionDetector& path_intersection_detector,
    const PathPlannerDesc& desc)
    :   m_unimpended_path_finder(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            desc.unimpended_path_window_size,
            desc.unimpended_path_max_astar_points
        ),
        m_path_intersection_detector(&path_intersection_detector),
        m_occupancy_grid(
            engine.physical_device(),
            engine.device(),
            voxel_grid,
            manager_bundle.compute_pass_manager()
        ),
        m_planner(
            m_occupancy_grid,
            desc.nonholonomic_astar_desc,
            m_unimpended_path_finder
        )
{
    LOG_METHOD();

    logger().check(desc.unimpended_path_window_size > 1, "Unimpended path window size must be greater than 1");
    logger().check(desc.unimpended_path_max_astar_points > 0, "Unimpended path max astar points must be greater than 0");
}

PathPlanner::~PathPlanner() noexcept {
    stop();
}

void PathPlanner::start(VulkanSubmitContext&& submit_context) {
    LOG_METHOD();

    {
        std::lock_guard lock(m_request_mutex);
        if (m_running)
            return;

        m_running = true;
    }

    m_thread = std::thread(&PathPlanner::planner_loop, this, std::move(submit_context));
}

void PathPlanner::stop() noexcept {
    {
        std::lock_guard lock(m_request_mutex);
        if (!m_running)
            return;

        m_running = false;
    }

    m_request_cv.notify_all();

    if (m_thread.joinable())
        m_thread.join();
}

void PathPlanner::request_path_replan(NonholonomicPos start_pos, NonholonomicPos goal_pos) {
    LOG_METHOD();

    {
        std::lock_guard lock(m_request_mutex);
        m_requested_start = start_pos;
        m_requested_goal = goal_pos;
        m_replan_requested = true;
    }

    m_request_cv.notify_one();
}

bool PathPlanner::request_is_path_impended(VulkanSubmitContext& submit_context) {
    LOG_METHOD();

    std::vector<glm::vec3> path_points = current_path_points();
    return path_points.empty() ||
           m_path_intersection_detector->has_intersection(submit_context, path_points);
}

void PathPlanner::plan_path(
    VulkanSubmitContext& submit_context,
    NonholonomicPos start_pos,
    NonholonomicPos goal_pos)
{
    LOG_METHOD();

    std::lock_guard planning_lock(m_planning_mutex);

    AvgTimer total_path_finding_time;
    total_path_finding_time.start();

    m_planner.initialize(submit_context, start_pos, goal_pos);
    m_planner.find_nonholomic_path();

    total_path_finding_time.end();

    {
        std::lock_guard result_lock(m_result_mutex);
        m_result.plain_astar_path = m_planner.state().plain_astar_path;
        m_result.nonholonomic_astar_path = m_planner.state().path;
        m_result.explored_paths = m_planner.state().explored_paths;
        m_result.unimpended_path = m_planner.state().unimpended_astar_positions;
        m_result.total_path_finding_time = total_path_finding_time;
        m_result.total_time_ms = static_cast<float>(total_path_finding_time.total_ms());
        m_result.initialize_time_ms = m_planner.initialize_time_ms();
        m_result.plain_astar_time_ms = m_planner.plain_astar_time_ms();
        m_result.unimpended_path_time_ms = m_planner.unimpended_path_time_ms();
        m_result.nonholonomic_astar_time_ms = m_planner.nonholonomic_astar_time_ms();
        m_result.is_solid_time_ms = m_planner.is_solid_time_ms();
        m_result.is_solid_count = m_planner.is_solid_count();
        m_result.generation++;
    }
}

PathPlanner::PathPlannerResult PathPlanner::request_result_snapshot() const {
    std::lock_guard lock(m_result_mutex);
    return m_result;
}

uint64_t PathPlanner::request_result_generation() const noexcept {
    std::lock_guard lock(m_result_mutex);
    return m_result.generation;
}

bool PathPlanner::request_has_path() const {
    std::lock_guard lock(m_result_mutex);
    return !m_result.nonholonomic_astar_path.empty();
}

bool PathPlanner::request_adjust_to_ground(
    glm::vec3& output,
    int max_step_up,
    int max_drop,
    int max_y_diff,
    bool allow_flying_over_precepices,
    uint32_t* status)
{
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.adjust_to_ground(
        output,
        max_step_up,
        max_drop,
        max_y_diff,
        allow_flying_over_precepices,
        status
    );
}

bool PathPlanner::request_is_solid(glm::ivec3 voxel_pos) {
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.is_solid(voxel_pos);
}

bool PathPlanner::request_is_solid_world(glm::vec3 point) {
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.is_solid(m_occupancy_grid.world_to_voxel_pos(point));
}

glm::vec3 PathPlanner::request_voxel_size() {
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.voxel_size();
}

glm::ivec3 PathPlanner::request_world_to_voxel_pos(const glm::vec3& point) {
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.world_to_voxel_pos(point);
}

glm::vec3 PathPlanner::request_voxel_to_world_pos(const glm::ivec3& voxel_pos) {
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.voxel_to_world_pos(voxel_pos);
}

glm::vec3 PathPlanner::request_voxel_center_world_pos(const glm::ivec3& voxel_pos) {
    std::lock_guard lock(m_occupancy_grid_mutex);
    return m_occupancy_grid.voxel_center_world_pos(voxel_pos);
}

void PathPlanner::planner_loop(VulkanSubmitContext submit_context) {
    LOG_METHOD();

    while (true) {
        NonholonomicPos start_pos;
        NonholonomicPos goal_pos;

        {
            std::unique_lock lock(m_request_mutex);
            m_request_cv.wait(lock, [&] {
                return !m_running || m_replan_requested;
            });

            if (!m_running)
                break;

            start_pos = m_requested_start;
            goal_pos = m_requested_goal;
            m_replan_requested = false;
        }

        plan_path(submit_context, start_pos, goal_pos);
    }
}

std::vector<glm::vec3> PathPlanner::current_path_points() const {
    std::lock_guard lock(m_result_mutex);

    std::vector<glm::vec3> path_points;
    path_points.reserve(m_result.nonholonomic_astar_path.size());

    for (const NonholonomicPos& point : m_result.nonholonomic_astar_path) {
        path_points.push_back(point.pos);
    }

    return path_points;
}
