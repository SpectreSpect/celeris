#include "global_planner_block.h"

#include "../../../vulkan_self/vulkan_engine.h"
#include "../../../managers/manager_bundle.h"
#include "sensor_reciever_block.h"

GlobalPlannerBlock::GlobalPlannerBlock(
    VulkanEngine& engine,
    ManagerBundle& manager_bundle,
    VulkanSubmitContext& submit_context,
    VoxelGrid& voxel_grid,
    const VehicleGeometry& vehicle_geometry,
    const Desc& desc)
    :   m_vehicle_geometry(&vehicle_geometry),
        m_voxel_grid(&voxel_grid),
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

void GlobalPlannerBlock::start(VulkanSubmitContext&& planner_submit_context) {
    LOG_METHOD();
    m_collision_escape_resolver.reset();
    m_path_planner.start(std::move(planner_submit_context));
}

void GlobalPlannerBlock::update(
    VulkanSubmitContext& submit_context, 
    SensorRecieverBlock& sensor_reciever_block) 
{
    LOG_METHOD();

    sync_path_snapshot();

    if (path_replan_required(submit_context, sensor_reciever_block)) {
        update_start_position(sensor_reciever_block);
        request_path_replan();
    }
}

void GlobalPlannerBlock::request_path_replan() {
    LOG_METHOD();

    m_path_planner.request_path_replan(m_start_position, m_goal_position);
}

void GlobalPlannerBlock::update_start_position(SensorRecieverBlock& sensor_reciever_block) {
    LOG_METHOD();

    logger().check(m_vehicle_geometry, "Vehicle geometry was null");
    logger().check(m_voxel_grid, "Voxel grid was null");

    if (!sensor_reciever_block.has_lidar_transform())
        return;

    const Transform& transform = *sensor_reciever_block.lidar_tranform();
    
    m_start_position.pos = m_vehicle_geometry->rear_axle_world_position(transform);
    m_start_position.pos.y += m_voxel_grid->voxel_size().y * 0.5f;

    m_start_position.theta = NonholonomicPos::from_transform(transform).theta;
    m_collision_escape_resolver.push_out(m_start_position.pos);
}

void GlobalPlannerBlock::set_start(const NonholonomicPos& position) {
    m_start_position = position;
    m_collision_escape_resolver.reset();
}

void GlobalPlannerBlock::set_goal(const NonholonomicPos& position) {
    m_goal_position = position;
}

bool GlobalPlannerBlock::adjust_to_ground(
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

NonholonomicPos GlobalPlannerBlock::start_position() const noexcept {
    return m_start_position;
}

NonholonomicPos GlobalPlannerBlock::goal_position() const noexcept {
    return m_goal_position;
}

const PathPlanner::PathPlannerResult& GlobalPlannerBlock::path_snapshot() const noexcept {
    return m_path_snapshot;
}

void GlobalPlannerBlock::sync_path_snapshot() {
    if (m_path_snapshot.generation != m_path_planner.request_result_generation())
        m_path_snapshot = m_path_planner.request_result_snapshot();
}

bool GlobalPlannerBlock::path_replan_required(
    VulkanSubmitContext& submit_context, 
    SensorRecieverBlock& sensor_reciever_block) 
{
    return 
        sensor_reciever_block.has_lidar_transform() && 
        m_path_planner.request_is_path_impended(submit_context);
}
