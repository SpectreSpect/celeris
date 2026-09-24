#pragma once

class SteeringWheel {
public:
    struct Desc {
        float max_angle = 0.7f;
        float max_velocity = 1.0f;
        float max_acceleration = 5.0f;
    };

    SteeringWheel(const Desc& desc);

    // void set_position(float position, float velocity, float acceleration);
    // void set_velocity(float velocity, float acceleration);
    void set_acceleration(float acceleration);

    void update(float delta_time);

private:
    Desc m_desc;

    float m_angle = 0.0f;
    float m_velocity = 0.0f;
    float m_acceleration = 0.0f;
};