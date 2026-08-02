#include "odometry_estimator.h"

OdometryEstimator::OdometryEstimator() {
}

Odometry OdometryEstimator::get_latest_odometry() {
    if (m_history.empty())
        return Odometry{};
    return m_history.back();
}

void OdometryEstimator::submit_imu(const ImuMeasurement& imu_measurement) {
    Odometry new_odometry{};
    if (m_history.empty()) {
        new_odometry.timestamp_ns = imu_measurement.timestamp;
        new_odometry.angular_velocity = imu_measurement.angular_velocity;
        new_odometry.linear_acceleration = glm::vec3(0.0f);

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
            const glm::quat delta_rotation = glm::angleAxis(angle, axis);

            last_odometry.orientation =
                glm::normalize(last_odometry.orientation * delta_rotation);
        }

        glm::vec3 acceleration_world = last_odometry.orientation * imu_measurement.linear_acceleration - m_gravity_world;
        last_odometry.linear_acceleration = acceleration_world;

        // last_odometry.position += last_odometry.linear_velocity * dt;
        last_odometry.position += last_odometry.linear_velocity * dt + 0.5f * acceleration_world * dt * dt;

        last_odometry.linear_velocity += acceleration_world * dt;

        last_odometry.timestamp_ns = imu_measurement.timestamp;

        new_odometry = last_odometry;
    }

    m_history.push_back(new_odometry);
}