#include "odometry_estimator.h"

#include <iostream>

#include "../sensors/lidar/lidar_scan.h"

OdometryEstimator::OdometryEstimator() {
}

Odometry OdometryEstimator::get_latest_odometry() {
    if (m_history.empty())
        return Odometry{};
    return m_history.back();
}

void OdometryEstimator::submit_imu(const ImuMeasurement& imu_measurement) {
    if (!m_history.empty() &&
        imu_measurement.timestamp <= m_history.back().timestamp_ns) {
        return;
    }

    // if (calibration_step < max_calibration_steps) {
    //     m_gravity_total += imu_measurement.linear_acceleration;
    //     calibration_step++;
    //     std::cout << "Calibration step: " << calibration_step << std::endl;
    //     return;
    // } else if (calibration_step == max_calibration_steps) {
    //     m_gravity_world = m_gravity_total / static_cast<float>(calibration_step);
    // }


    // std::cout << "Linear acceleration: (" << 
    //     imu_measurement.linear_acceleration.x << ", " <<
    //     imu_measurement.linear_acceleration.y << ", " <<
    //     imu_measurement.linear_acceleration.z << ")" << std::endl;

    // m_gravity_total += imu_measurement.linear_acceleration;
    // gravity_count += 1;
    // glm::vec3 mean_gravity = m_gravity_total / static_cast<float>(gravity_count);
    // std::cout << "Mean gravity: (" << 
    //         mean_gravity.x << ", " <<
    //         mean_gravity.y << ", " <<
    //         mean_gravity.z << ")" << std::endl;
        

    Odometry new_odometry{};
    if (m_history.empty()) {
        new_odometry.timestamp_ns = imu_measurement.timestamp;
        new_odometry.angular_velocity = imu_measurement.angular_velocity;
        new_odometry.linear_acceleration = glm::vec3(0.0f);

        // m_gravity_world = glm::vec3(-0.202349f, 10.0953f, -0.240715f);
        m_gravity_world = imu_measurement.linear_acceleration;
    } else {
        Odometry last_odometry = m_history.back();

        const float dt =
        static_cast<float>(imu_measurement.timestamp - last_odometry.timestamp_ns)
        * 1e-9f;

        if (dt <= 0.0f)
            return;
        
        last_odometry.angular_velocity = imu_measurement.angular_velocity;

        const glm::vec3 rotation = imu_measurement.angular_velocity * dt;
        const float angle = glm::length(rotation);

        if (angle > 1e-6f) {
            const glm::vec3 axis = rotation / angle;
            // std::cout << "Angle: " << angle << std::endl;
            const glm::quat delta_rotation = glm::angleAxis(angle, axis);

            last_odometry.orientation =
                glm::normalize(last_odometry.orientation * delta_rotation);
        }

        glm::vec3 acceleration_world = last_odometry.orientation * imu_measurement.linear_acceleration - m_gravity_world;
        last_odometry.linear_acceleration = acceleration_world;

        // if (glm::length(acceleration_world) <= m_max_gravity_length)
        //     return;

        // if (m_max_gravity_length < glm::length(acceleration_world)) {
        //     m_max_gravity_length = glm::length(acceleration_world);
        //     std::cout << "Max gravity length: " << m_max_gravity_length << std::endl;
        // }

        // std::cout << "Linear acceleration: (" << 
        //     acceleration_world.x << ", " <<
        //     acceleration_world.y << ", " <<
        //     acceleration_world.z << ")" << std::endl;

        // last_odometry.position += last_odometry.linear_velocity * dt;
        last_odometry.position += last_odometry.linear_velocity * dt + 0.5f * acceleration_world * dt * dt;

        last_odometry.linear_velocity += acceleration_world * dt;

        last_odometry.timestamp_ns = imu_measurement.timestamp;

        new_odometry = last_odometry;
    }

    m_history.push_back(new_odometry);
}

void OdometryEstimator::submit_lidar_scan(LidarScan& lidar_scan, const Odometry& closest_prev_odometry) {
    Odometry new_odometry{};

    new_odometry.position = lidar_scan.point_cloud().transform.position;
    new_odometry.orientation = lidar_scan.point_cloud().transform.rotation;
    new_odometry.timestamp_ns = lidar_scan.timestamp();

    const float dt =
        static_cast<float>(new_odometry.timestamp_ns - closest_prev_odometry.timestamp_ns)
        * 1e-9f;

    if (dt > 1e-6f) {
        new_odometry.linear_velocity =
            (new_odometry.position - closest_prev_odometry.position) / dt;

        glm::quat delta_rotation = glm::normalize(
            new_odometry.orientation * glm::inverse(closest_prev_odometry.orientation)
        );

        // Select the shortest equivalent rotation.
        if (delta_rotation.w < 0.0f)
            delta_rotation = -delta_rotation;

        const float angle = glm::angle(delta_rotation);
        const glm::vec3 axis = glm::axis(delta_rotation);

        new_odometry.angular_velocity =
            angle > 1e-6f ? axis * (angle / dt) : glm::vec3(0.0f);
        
        new_odometry.linear_acceleration = (
            new_odometry.linear_velocity - 
            closest_prev_odometry.linear_velocity) / dt;
    }

    submit_odometry(new_odometry);
}

void OdometryEstimator::submit_odometry(const Odometry& odometry) {
    while (!m_history.empty() &&
           m_history.back().timestamp_ns >= odometry.timestamp_ns) {
        m_history.pop_back();
    }

    m_history.push_back(odometry);
}

bool OdometryEstimator::get_closest_prev_odometry(uint64_t timestamp, Odometry& output) {
    for (int i = m_history.size() - 1; i >= 0; i--) {
        
        if (m_history[i].timestamp_ns < timestamp) {
            output = m_history[i];
            return true;
        }
    }
    return false;
}