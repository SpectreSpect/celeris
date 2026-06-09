#pragma once
#include "sphere.h"
#include "leg.h"

class Spider : public Drawable, public Transformable {
public:
    Sphere body;
    std::vector<Leg*> legs;
    std::vector<bool> moving_limb;
    std::vector<bool> first_forward;
    std::vector<float> movement_start_delay;
    float time_since_movment_start = 0.0f;
    glm::vec3 velocity = glm::vec3(0, 0, 0);
    glm::vec3 prev_velocity = glm::vec3(0, 0, 0);

    Spider(int num_legs, int num_bones, float bone_length);
    void update(float delta_time);
    virtual void draw(RenderState state);
};