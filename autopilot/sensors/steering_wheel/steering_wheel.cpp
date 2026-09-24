#include "steering_wheel.h"

#include <algorithm>

SteeringWheel::SteeringWheel(const Desc& desc) 
    :   m_desc(desc) {}

void SteeringWheel::set_acceleration(float acceleration) {
    acceleration = std::clamp(acceleration, -m_desc.max_acceleration, m_desc.max_acceleration);
}

void SteeringWheel::update(float delta_time) {
    


    m_angle += m_velocity * delta_time;
    m_velocity += m_acceleration * delta_time;

    m_angle = std::clamp(m_angle, -m_desc.max_angle, m_desc.max_angle);
    m_velocity = std::clamp(m_velocity, -m_desc.max_velocity, m_desc.max_velocity);
}