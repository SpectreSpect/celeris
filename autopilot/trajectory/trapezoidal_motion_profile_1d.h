#pragma once

#include "../../vulkan_self/logger/logger_header.h"

class TrapezoidalMotionProfile1D {
public:
    _XPARENT_NAME(TrapezoidalMotionProfile1D);

    TrapezoidalMotionProfile1D(
        float acceleration_1,
        float velocity,
        float acceleration_2
    );
    void plan(float start_pos, float end_pos);
    float position(float t);

private:
    float m_acceleration_1 = 0;
    float m_velocity = 0;
    float m_acceleration_2 = 0;

    float m_start_pos = 0;
    float m_end_pos = 0;

    bool m_is_planned = false;
    
    float m_max_velocity = 0;
    float m_t_a = 0;
    float m_t_d = 0;
    float m_t_2 = 0;

    float position_1(float t);
    float position_2(float t);
    float position_3(float t);
};