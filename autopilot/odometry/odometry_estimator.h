#pragma once

#include <deque>

#include "../../vulkan_self/logger/logger_header.h"
#include "../sensors/imu/imu_measurement.h"
#include "odometry.h"

class LidarScan;

class OdometryEstimator {
public:
    _XCLASS_NAME(OdometryEstimator);

    OdometryEstimator();

    void start_gravity_calibration(uint32_t calibration_step_count);
    bool is_gravity_calibration_underway();
    void gravity_calibration_step(const ImuMeasurement& imu_measurement);
    void set_gravity(glm::vec3 gravity);

    Odometry get_latest_odometry();

    void submit_imu(const ImuMeasurement& imu_measurement);
    void submit_lidar_scan(LidarScan& lidar_scan, const Odometry& closest_prev_odometry);
    void submit_lidar_imu_fusion(
        LidarScan& lidar_scan, 
        const Odometry& closest_prev_imu_odometry, 
        const Odometry& last_lidar_odometry
    );
    void submit_odometry(const Odometry& odometry);

    bool get_closest_prev_odometry(uint64_t timestamp, Odometry& output);

    bool get_last_lidar_odometry(Odometry& output);
    bool get_last_imu_odometry(Odometry& output);

    size_t history_size();
    Odometry get_odometry(uint32_t id);

private:
    std::deque<Odometry> m_history;
    glm::vec3 m_gravity;

    uint32_t m_gravity_calibration_step = 1;
    uint32_t m_gravity_calibration_step_count = 0;
    glm::vec3 m_gravity_total;
};