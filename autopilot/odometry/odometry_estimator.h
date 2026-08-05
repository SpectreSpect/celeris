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
    void submit_odometry(const Odometry& odometry);

    bool get_closest_prev_odometry(uint64_t timestamp, Odometry& output);

private:
    std::deque<Odometry> m_history;
    glm::vec3 m_gravity_world;

    // uint32_t calibration_step = 0;
    // uint32_t max_calibration_steps = 100;

    // glm::vec3 m_gravity_total;
    // float m_max_gravity_length = 0.422485f;
    // uint32_t gravity_count = 0;
};