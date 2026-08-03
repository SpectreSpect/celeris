#pragma once

#include <deque>

#include "../sensors/imu/imu_measurement.h"
#include "odometry.h"

class NewLidarScan;

class OdometryEstimator {
public:
    OdometryEstimator();

    Odometry get_latest_odometry();

    void submit_imu(const ImuMeasurement& imu_measurement);
    void submit_lidar_scan(NewLidarScan& lidar_scan, const Odometry& closest_prev_odometry);
    void submit_odometry(const Odometry& odometry);

    bool get_closest_prev_odometry(uint64_t timestamp, Odometry& output);

private:
    std::deque<Odometry> m_history;
    glm::vec3 m_gravity_world;
};