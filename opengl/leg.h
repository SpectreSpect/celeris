#pragma once
#include "limb.h"


class Leg : public Limb{
public:
    float swing_time;
    float swing_timer = 0.0f;
    float step_size;
    glm::vec3 step_middle;
    glm::vec3 swing_start_position;
    glm::vec3 swing_end_position;
    bool swinging = false;
    bool moving;
    
    Leg(int num_bones, std::vector<float> bone_lengths, glm::vec3 step_middle, float swing_time, float step_size, bool moving);
    void swing(glm::vec3 swing_position);
    void update(float delta_time);   
};