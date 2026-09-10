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
        m_sensor_receiver_block(
            engine, 
            manager_bundle,
            compute_queue,
            desc.sensors
        ),
        m_point_map_block(engine, manager_bundle, desc.point_map),
        m_global_planner_block(
            engine, 
            manager_bundle, 
            submit_context, 
            voxel_grid, 
            vehicle_geometry, 
            desc.global_planner
        ){
}

void NewCeleris::start(VulkanSubmitContext&& planner_submit_context) {
    LOG_METHOD();

    m_sensor_receiver_block.start();
    m_global_planner_block.start(std::move(planner_submit_context));
}

void NewCeleris::update(VulkanSubmitContext& submit_context) {
    LOG_METHOD();

    m_sensor_receiver_block.update(m_point_map_block, m_voxel_grid);
    m_global_planner_block.update(submit_context, m_sensor_receiver_block);
}

void NewCeleris::update_global_planner_start_position() {
    m_global_planner_block.update_start_position(
        m_sensor_receiver_block
    );
}

OdometryEstimator& NewCeleris::odometry_estimator() {
    return m_sensor_receiver_block.odometry_estimator();
}

bool NewCeleris::has_lidar_transform() {
    return m_sensor_receiver_block.has_lidar_transform();
}

Transform* NewCeleris::lidar_tranform() {
    return m_sensor_receiver_block.lidar_tranform();
}

VoxelGrid* NewCeleris::voxel_grid() {
    return m_voxel_grid;
}

const PathPlanner::PathPlannerResult& NewCeleris::global_path_snapshot() const noexcept {
    return m_global_planner_block.path_snapshot();
}

GlobalPlannerBlock& NewCeleris::global_planner() noexcept {
    return m_global_planner_block;
}

SensorReceiverBlock& NewCeleris::sensor_receiver_block() noexcept {
    return m_sensor_receiver_block;
}
