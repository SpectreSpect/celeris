#include "spider.h"

Spider::Spider(int num_legs, int num_bones, float bone_length) {
    float pi = glm::pi<float>();
    std::vector<float> bone_lengths(num_bones, bone_length);
    body.mesh->position = glm::vec3(0, 2, 0);
    for (int i = 0; i < num_legs; i++) {
        float angle = ((float)i / num_legs) * 2.0f * pi;
        glm::vec3 dir = glm::normalize(glm::vec3(glm::cos(angle), 0, glm::sin(angle)));
        glm::vec3 initial_end_dir = dir;
        initial_end_dir.y = 0;

        float step_size = 2;
        glm::vec3 middle_step = initial_end_dir * 5.0f;

        bool moving = i % 2 == 0;
        
        float step_time = 0.2f;

        Leg* leg = new Leg(num_bones, bone_lengths, middle_step, step_time, step_size, moving);
        leg->start = body.mesh->position;
        legs.push_back(leg);
        moving_limb.push_back(false);
        first_forward.push_back(i % 2 == 0);

        if (moving)
            movement_start_delay.push_back(0.0f);
        else
            movement_start_delay.push_back(-1.0f);
    }
}

void Spider::update(float delta_time) {
    position += velocity * delta_time;

    for (int i = 0; i < legs.size(); i++) {
        Leg& leg = *(legs[i]);

        float step_size = 5;
        glm::vec3 movement_dir = glm::normalize(velocity);
        glm::vec3 step_front = leg.step_middle + movement_dir * step_size * 0.5f;

        bool stated_moving = false;
        if (glm::length(prev_velocity) == 0.0f && glm::length(velocity) != 0.0f)
            stated_moving = true;
        
        
        if (glm::length(velocity) > 0.0f)
            if (movement_start_delay[i] >= 0.0f)
                if (time_since_movment_start <= movement_start_delay[i] && time_since_movment_start + delta_time >= movement_start_delay[i])
                    leg.swing(step_front);

        if (!leg.swinging) {
            leg.end -= velocity * delta_time;

            float movement_speed = glm::length(velocity) * delta_time;

            if (movement_speed == 0.0) {

            } else {
                
                float dist_to_step_front = glm::distance(leg.end, step_front);
                if (dist_to_step_front > step_size) {
                    leg.swing(step_front);
                }
            }
        }

        leg.update(delta_time);
    }

    prev_velocity = velocity;

    if (glm::length(velocity) != 0.0f)
        time_since_movment_start += delta_time;
    if (glm::length(velocity) == 0.0f)
        time_since_movment_start = 0.0f;
    else if (glm::length(prev_velocity) == 0.0f && glm::length(velocity) != 0.0f)
        time_since_movment_start = 0.0f;
}

void Spider::draw(RenderState state) {
    state.transform *= get_model_matrix();

    body.mesh->draw(state);
    for (int i = 0; i < legs.size(); i++) {
        legs[i]->draw(state);
    }
}