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
        
    // std::cout << "Imu timestamp: " << imu_measurement.timestamp * 1e-9 << std::endl;

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

    new_odometry.sensor_type = TYPE_IMU;

    m_history.push_back(new_odometry);
}

void OdometryEstimator::submit_lidar_scan(LidarScan& lidar_scan, const Odometry& closest_prev_odometry) {
    Odometry new_odometry{};

    new_odometry.position = lidar_scan.point_cloud().transform.position;
    new_odometry.orientation = lidar_scan.point_cloud().transform.rotation;
    new_odometry.timestamp_ns = lidar_scan.timestamp();

    // std::cout << "Lidar timestamp: " << new_odometry.timestamp_ns * 1e-9 << std::endl;

    const float dt =
        static_cast<float>(new_odometry.timestamp_ns - closest_prev_odometry.timestamp_ns)
        * 1e-9f;

    if (dt > 1e-6f) {
        new_odometry.linear_velocity =
            (new_odometry.position - closest_prev_odometry.position) / dt;
        
        std::cout << "new_domotery.position: (" << 
            new_odometry.position.x << ", " << 
            new_odometry.position.y << ", " << 
            new_odometry.position.z << ")   closest_prev_odometry.position: (" << 
            closest_prev_odometry.position.x << ", " << 
            closest_prev_odometry.position.y << ", " <<
            closest_prev_odometry.position.z << ")  dt: " <<
            dt << "     new_linear_velocity: (" <<
            new_odometry.linear_velocity.x << ", " <<
            new_odometry.linear_velocity.y << ", " << 
            new_odometry.linear_velocity.z << ")" << std::endl;

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

    new_odometry.sensor_type = TYPE_LIDAR;

    // std::cout << "Lidar odometry position: " << new_odometry.position.x << ", " << new_odometry.position.y << ", " << new_odometry.position.z << ")" << std::endl;

    submit_odometry(new_odometry);
}

void OdometryEstimator::submit_lidar_imu_fusion(
        LidarScan& lidar_scan, 
        const Odometry& closest_prev_imu_odometry, 
        const Odometry& last_lidar_odometry
) {
    constexpr float alpha = 0.5f;
    constexpr float beta = 0.05f;

    const std::int64_t lidar_timestamp_ns =
        static_cast<std::int64_t>(lidar_scan.timestamp());

    Odometry fused_odometry = closest_prev_imu_odometry;
    fused_odometry.timestamp_ns = lidar_timestamp_ns;
    fused_odometry.sensor_type = TYPE_LIDAR;

    const glm::vec3 lidar_position =
        lidar_scan.point_cloud().transform.position;
    const glm::vec3 position_innovation =
        lidar_position - closest_prev_imu_odometry.position;

    fused_odometry.position =
        closest_prev_imu_odometry.position + alpha * position_innovation;

    if (last_lidar_odometry.timestamp_ns > 0 &&
        lidar_timestamp_ns > last_lidar_odometry.timestamp_ns) {
        const float lidar_dt = static_cast<float>(
            lidar_timestamp_ns - last_lidar_odometry.timestamp_ns
        ) * 1e-9f;

        if (lidar_dt > 1e-6f) {
            fused_odometry.linear_velocity =
                closest_prev_imu_odometry.linear_velocity +
                beta * position_innovation / lidar_dt;
        }
    }

    fused_odometry.orientation =
        lidar_scan.point_cloud().transform.rotation;

    submit_odometry(fused_odometry);
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

bool OdometryEstimator::get_last_lidar_odometry(Odometry& output) {
    for (int i = m_history.size() - 1; i >= 0; i--) {
        
        if (m_history[i].sensor_type == TYPE_LIDAR) {
            output = m_history[i];
            return true;
        }
    }
    return false;
}

bool OdometryEstimator::get_last_imu_odometry(Odometry& output) {
    for (int i = m_history.size() - 1; i >= 0; i--) {
        
        if (m_history[i].sensor_type == TYPE_IMU) {
            output = m_history[i];
            return true;
        }
    }
    return false;
}

size_t OdometryEstimator::history_size() {
    return m_history.size();
}

Odometry OdometryEstimator::get_odometry(uint32_t id) {
    return m_history[id];
}
