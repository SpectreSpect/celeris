#include "new_celeris.h"

#include "../../vulkan_self/vulkan_submit_context.h"
#include "../../voxel_grid_vulkan/voxel_grid.h"
#include "../../vulkan_self/vulkan_engine.h"
#include "../../managers/manager_bundle.h"
#include "../sensors/lidar/lidar_scan.h"
#include "../../renderer/transform.h"

NewCeleris::NewCeleris(
    VulkanEngine& engine,
    ManagerBundle& manager_bundle, 
    VulkanQueue& compute_queue,
    VulkanSubmitContext& submit_context,
    VoxelGrid& voxel_grid,
    const VehicleGeometry& vehicle_geometry,
    const Desc& desc)
    :   m_engine(&engine),
        m_voxel_grid(&voxel_grid),
        m_desc(desc),
        m_vehicle_geometry(vehicle_geometry),
        m_sensor_reciever_block(
            engine, 
            manager_bundle,
            compute_queue,
            desc.sensors
        ),
        m_point_map_block(engine, manager_bundle, desc.point_map),
        m_path_intersection_detector(
            engine.physical_device(),
            engine.device(),
            submit_context,
            manager_bundle.compute_pass_manager(),
            voxel_grid,
            desc.max_intersection_path_points
        ),
        m_path_planner(
            engine,
            submit_context,
            manager_bundle,
            voxel_grid,
            m_path_intersection_detector,
            vehicle_geometry,
            desc.planner
        ),
        m_collision_escape_resolver(
            m_path_planner,
            desc.collision_escape
        ) {
}

void NewCeleris::start(VulkanSubmitContext&& planner_submit_context) {
    LOG_METHOD();

    m_collision_escape_resolver.reset();
    m_sensor_reciever_block.start();
    m_path_planner.start(std::move(planner_submit_context));
}

void NewCeleris::update(VulkanSubmitContext& submit_context) {
    LOG_METHOD();

    if (m_path_planner_snapshot.generation != m_path_planner.request_result_generation())
        m_path_planner_snapshot = m_path_planner.request_result_snapshot();
    
    m_sensor_reciever_block.update(m_point_map_block, m_voxel_grid);

    if (path_replan_required(submit_context)) {
        update_start_position();
        request_path_replan();
    }
}

void NewCeleris::set_start(const NonholonomicPos& position) {
    m_start_position = position;
    m_collision_escape_resolver.reset();
}

void NewCeleris::set_goal(const NonholonomicPos& position) {
    m_goal_position = position;
}

bool NewCeleris::adjust_to_ground(
    glm::vec3& output,
    int max_step_up,
    int max_drop,
    int max_y_diff,
    bool allow_flying_over_precepices) {
    return m_path_planner.request_adjust_to_ground(
        output,
        max_step_up,
        max_drop,
        max_y_diff,
        allow_flying_over_precepices
    );
}

void NewCeleris::request_path_replan() {
    LOG_METHOD();

    m_path_planner.request_path_replan(m_start_position, m_goal_position);
}

void NewCeleris::update_start_position() {
    if (!has_lidar_transform())
        return;

    const Transform& transform = *lidar_tranform();
    
    m_start_position.pos = m_vehicle_geometry.rear_axle_world_position(transform);
    m_start_position.pos.y += voxel_grid()->voxel_size().y * 0.5f;

    m_start_position.theta = NonholonomicPos::from_transform(transform).theta;
    m_collision_escape_resolver.push_out(m_start_position.pos);
}

OdometryEstimator& NewCeleris::odometry_estimator() {
    return m_sensor_reciever_block.odometry_estimator();
}

bool NewCeleris::has_lidar_transform() {
    return m_sensor_reciever_block.has_lidar_transform();
}

Transform* NewCeleris::lidar_tranform() {
    return m_sensor_reciever_block.lidar_tranform();
}

VoxelGrid* NewCeleris::voxel_grid() {
    return m_voxel_grid;
}

glm::vec3 NewCeleris::voxel_center_bottom_world_pos(const glm::ivec3& voxel_pos) {
    logger().check(m_voxel_grid, "Voxel grid was null");
    
    // return m_path_planner.request_voxel_center_world_pos(voxel_pos);
    return (glm::vec3(voxel_pos) + glm::vec3(0.5f, 0.0f, 0.5f)) * m_voxel_grid->voxel_size();
}

glm::vec3 NewCeleris::voxel_center_world_pos(const glm::ivec3& voxel_pos) {
    logger().check(m_voxel_grid, "Voxel grid was null");
    
    // return m_path_planner.request_voxel_center_world_pos(voxel_pos);
    return (glm::vec3(voxel_pos) + glm::vec3(0.5f)) * m_voxel_grid->voxel_size();
}

NonholonomicPos NewCeleris::start_position() const noexcept {
    return m_start_position;
}

NonholonomicPos NewCeleris::goal_position() const noexcept {
    return m_goal_position;
}

const PathPlanner::PathPlannerResult& NewCeleris::path_planner_snapshot() const noexcept {
    return m_path_planner_snapshot;
}

bool NewCeleris::path_replan_required(VulkanSubmitContext& submit_context) {
    return has_lidar_transform() && m_path_planner.request_is_path_impended(submit_context);
}
