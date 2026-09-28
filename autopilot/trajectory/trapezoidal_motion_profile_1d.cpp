#include "trapezoidal_motion_profile_1d.h"

#include <cmath>

TrapezoidalMotionProfile1D::TrapezoidalMotionProfile1D(
    float acceleration_1,
    float velocity,
    float acceleration_2) 
    :   m_acceleration_1(acceleration_1),
        m_velocity(velocity),
        m_acceleration_2(acceleration_2) {
    LOG_METHOD();

    logger().check(acceleration_1 != 0, "acceleration_1 must not be 0");
    logger().check(acceleration_2 != 0, "acceleration_2 must not be 0");
    logger().check(velocity != 0, "velocity must not be 0");
}

void TrapezoidalMotionProfile1D::plan(float start_pos, float end_pos) {
    m_start_pos = start_pos;
    m_end_pos = end_pos;

    m_max_velocity = m_velocity;
    if (m_end_pos >= m_start_pos) {
        float dist = m_end_pos - m_start_pos;
        float numerator = 2 * dist * m_acceleration_1 * std::abs(m_acceleration_2);
        float denominator = m_acceleration_1 + std::abs(m_acceleration_2);

        m_max_velocity = std::sqrt(numerator / denominator);
    }

    m_is_planned = true;
    
    float v = std::min(m_velocity, m_max_velocity);
    m_t_a = v / m_acceleration_1;
    m_t_d = m_t_a + (m_end_pos - position_1(m_t_a)) / v + v / (2 * m_acceleration_2);
    m_t_2 =  m_t_a + (m_end_pos - position_1(m_t_a)) / v - v / (2 * m_acceleration_2);
}

float TrapezoidalMotionProfile1D::position(float t) {
    LOG_METHOD();
    logger().check(m_is_planned, "The motion must be planned first");

    if (t <= 0)
        return m_start_pos;
    if (t > 0 && t <= m_t_a)
        return position_1(t);
    if (t > m_t_a && t <= m_t_d)
        return position_2(t);
    if (t > m_t_d && t < m_t_2)
        return position_3(t);
    if (t >= m_t_2)
        return m_end_pos;
    return std::numeric_limits<float>::max();
}

float TrapezoidalMotionProfile1D::duration() const {
    return m_t_2;
}

float TrapezoidalMotionProfile1D::position_1(float t) {
    LOG_METHOD();
    logger().check(m_is_planned, "The motion must be planned first");

    return m_start_pos + t * t * m_acceleration_1 / 2.0f;
}

float TrapezoidalMotionProfile1D::position_2(float t) {
    LOG_METHOD();
    logger().check(m_is_planned, "The motion must be planned first");

    return position_1(m_t_a) + (t - m_t_a) * std::min(m_velocity, m_max_velocity);
}

float TrapezoidalMotionProfile1D::position_3(float t) {
    LOG_METHOD();
    logger().check(m_is_planned, "The motion must be planned first");

    return m_end_pos + std::pow((t - m_t_2), 2) * m_acceleration_2 / 2.0f;
}
