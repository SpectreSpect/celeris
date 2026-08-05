#pragma once

#include <deque>

#include "../sensors/imu/imu_measurement.h"
#include "odometry.h"

class LidarScan;

class OdometryEstimator {
public:
    OdometryEstimator();

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
    glm::vec3 m_gravity_world;

    // uint32_t calibration_step = 0;
    // uint32_t max_calibration_steps = 100;

    // glm::vec3 m_gravity_total;
    // float m_max_gravity_length = 0.422485f;
    // uint32_t gravity_count = 0;
};