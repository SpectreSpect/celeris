#include "ackermann_dynamics.h"

#include <cmath>
#include <numbers>
#include <glm/glm.hpp>

#include "../../vulkan_self/utils.h"

AckermannDynamics::AckermannDynamics(double wheel_base, double max_steering_angle)
    : m_wheel_base(wheel_base), 
      m_max_steering_angle(max_steering_angle) 
{
    LOG_METHOD();

    logger().check(m_wheel_base > 0.0, "`wheel_base` must be positive.");
    logger().check(0.0 <= m_max_steering_angle, "`max_steering_angle` must be not less than zero.");
    logger().check(m_max_steering_angle < std::numbers::pi / 2.0, "`max_steering_angle` must be less than `pi/2` due to a singularity.");
}

VehicleState& AckermannDynamics::simulate_step_inplace(VehicleState& state, double dt) const {
    LOG_METHOD();

    logger().check(dt > 0.0, "`dt` must be positive.");
    
    const double forward_velocity0 = state.forward_velocity;
    const double heading0 = state.heading;
    const double steering_angle0 = std::clamp(
        state.steering_angle,
        -m_max_steering_angle,
        m_max_steering_angle
    );
    double steering_rate0 = state.steering_rate;
    if ((steering_angle0 <= -m_max_steering_angle + Utils::eps && steering_rate0 < 0.0) ||
        (steering_angle0 >= m_max_steering_angle - Utils::eps && steering_rate0 > 0.0))
    {
        steering_rate0 = 0.0;
    }

    const glm::dvec2 forward0 = glm::dvec2{std::cos(heading0), std::sin(heading0)};
    const glm::dvec2 velocity0 = forward0 * forward_velocity0;
    const double yaw_rate0 = forward_velocity0 * std::tan(steering_angle0) / m_wheel_base;

    const double forward_velocity1 = forward_velocity0 + state.forward_acceleration * dt;
    double steering_rate1 = steering_rate0 + state.steering_acceleration * dt;
    
    double steering_angle1 = steering_angle0 + steering_rate0 * dt + 0.5 * state.steering_acceleration * dt * dt;

    steering_angle1 = std::clamp(
        steering_angle1,
        -m_max_steering_angle,
        m_max_steering_angle
    );
    if ((steering_angle1 <= -m_max_steering_angle + Utils::eps && steering_rate1 < 0.0) ||
        (steering_angle1 >= m_max_steering_angle - Utils::eps && steering_rate1 > 0.0))
    {
        steering_rate1 = 0.0;
    }

    const double yaw_rate1 = forward_velocity1 * std::tan(steering_angle1) / m_wheel_base;
    const double heading1 = heading0 + 0.5 * (yaw_rate0 + yaw_rate1) * dt;

    const glm::dvec2 forward1 = glm::dvec2{std::cos(heading1), std::sin(heading1)};
    const glm::dvec2 velocity1 = forward1 * forward_velocity1;

    state.position += 0.5 * (velocity0 + velocity1) * dt;
    state.heading = std::remainder(heading1, 2.0 * std::numbers::pi);

    state.forward_velocity = forward_velocity1;
    // state.forward_acceleration = state.forward_acceleration; // Не изменилось
    state.steering_angle = steering_angle1;
    state.steering_rate = steering_rate1;
    // state.steering_acceleration = state.steering_acceleration; // Не изменилось

    return state;
}
