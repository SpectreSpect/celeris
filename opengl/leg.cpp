#include "leg.h"



Leg::Leg(int num_bones, std::vector<float> bone_lengths, 
         glm::vec3 step_middle, float swing_time, float step_size, bool moving) : Limb(num_bones, bone_lengths, glm::vec3(0, 1, 0)) {
    this->swing_time = swing_time;
    this->step_size = step_size;
    this->step_middle = step_middle;
    this->moving = moving;
    end = this->step_middle;
    init();
}

void Leg::swing(glm::vec3 swing_position) {
    swinging = true;
    this->swing_start_position = end;
    this->swing_end_position = swing_position;
    swing_timer = 0.0f;
}

void Leg::update(float delta_time) {
    if (swinging) {
        swing_timer += delta_time;
        if (swing_timer >= swing_time) {
            swing_timer = swing_time;
            swinging = false;
        }
        float t = swing_timer / swing_time;
        glm::vec3 to_swing_position = swing_end_position - swing_start_position;
        glm::vec3 up_dir = glm::vec3(0, 0.4f, 0);
        float pi = glm::pi<float>();
        end = swing_start_position + to_swing_position * ((std::cos(pi - t * pi) + 1) / 2.0f) + up_dir * std::sin(t * pi);
    }
    init();
    backward_pass();
    forward_pass();
    backward_pass();
    forward_pass();

    update_straight_line();
}   
