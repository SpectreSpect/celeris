#pragma once
#include "drawable.h"
#include "transformable.h"
#include "line.h"

class Limb : public Drawable, public Transformable {
public:
    std::vector<float> bone_lengths;
    float limb_length = 0;
    std::vector<LineInstance> bones;
    std::vector<LineInstance> staright_line_instances;
    glm::vec3 start = glm::vec3(0, 0, 0);
    glm::vec3 end = glm::vec3(1, 0, 0);
    glm::vec3 initial_dir;
    Line limb_line;
    Line straight_line;

    Limb(int num_bones, std::vector<float> bone_lengths, glm::vec3 dir);
    void init();
    void update_straight_line();
    void backward_pass();
    void forward_pass();
    virtual void draw(RenderState state);
};