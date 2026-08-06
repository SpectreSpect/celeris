#include "odometry_estimator.h"

#include <iostream>

#include "../sensors/lidar/lidar_scan.h"

OdometryEstimator::OdometryEstimator() {
}

void OdometryEstimator::start_gravity_calibration(uint32_t calibration_step_count) {
    m_gravity_calibration_step = 0;
    m_gravity_calibration_step_count = calibration_step_count;
    m_gravity_total = glm::vec3(0, 0, 0);
    m_gravity = glm::vec3(0, 0, 0);
}

bool OdometryEstimator::is_gravity_calibration_underway() {
    return m_gravity_calibration_step <= m_gravity_calibration_step_count;
}

void OdometryEstimator::gravity_calibration_step(const ImuMeasurement& imu_measurement) {
    LOG_METHOD();

    if (m_gravity_calibration_step < m_gravity_calibration_step_count) {
        m_gravity_total += imu_measurement.linear_acceleration;
        m_gravity_calibration_step++;
        logger().log() << "Gravity calibration step: " << 
            clr(std::to_string(m_gravity_calibration_step), LoggerPalette::blue) << "\n";
    } else if (m_gravity_calibration_step == m_gravity_calibration_step_count) {
        m_gravity = m_gravity_total / static_cast<float>(m_gravity_calibration_step);
        m_gravity_calibration_step++;

        logger().log() << "Mean gravity: " << 
            clr("(", LoggerPalette::blue) <<
            clr(std::to_string(m_gravity.x), LoggerPalette::blue) << clr(", ", LoggerPalette::blue) <<
            clr(std::to_string(m_gravity.y), LoggerPalette::blue) << clr(", ", LoggerPalette::blue)  <<
            clr(std::to_string(m_gravity.z), LoggerPalette::blue) << clr(")", LoggerPalette::blue)  << "\n";

    }
}

void OdometryEstimator::set_gravity(glm::vec3 gravity) {
    m_gravity = gravity;
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

    if (is_gravity_calibration_underway()) {
        gravity_calibration_step(imu_measurement);
        return;
    }

    Odometry new_odometry{};
    if (m_history.empty()) {
        new_odometry.timestamp_ns = imu_measurement.timestamp;
        new_odometry.angular_velocity = imu_measurement.angular_velocity;
        new_odometry.linear_acceleration = glm::vec3(0.0f);
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
            const glm::quat delta_rotation = glm::angleAxis(angle, axis);

            last_odometry.orientation =
                glm::normalize(last_odometry.orientation * delta_rotation);
        }

        glm::vec3 acceleration_world = 
            last_odometry.orientation * imu_measurement.linear_acceleration - m_gravity;
        last_odometry.linear_acceleration = acceleration_world;

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

    new_odometry.sensor_type = TYPE_LIDAR;

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
