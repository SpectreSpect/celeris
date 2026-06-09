#include "limb.h"


Limb::Limb(int num_bones, std::vector<float> bone_lengths, glm::vec3 dir) {
    this->bone_lengths = bone_lengths;
    for (int i = 0; i < this->bone_lengths.size(); i++) 
        limb_length += this->bone_lengths[i];
    
    num_bones = std::max(num_bones, 2);
    bones = std::vector<LineInstance>(num_bones, {glm::vec3(0), glm::vec3(0)});
    staright_line_instances = std::vector<LineInstance>(1, {glm::vec3(0), glm::vec3(0)});
    straight_line.color = glm::vec4(0, 0, 1, 1);
    limb_line.color = glm::vec4(0.564705882f, 0.835294118f, 1.0f, 1.0f);
    initial_dir = dir;
    init();
}

void Limb::init() {
    glm::vec3 last_joint = start;
    initial_dir = glm::normalize(initial_dir);
    for (int i = 0; i < bones.size(); i++) {
        glm::vec3 limb_end = last_joint;
        limb_end = last_joint + initial_dir * bone_lengths[i];
        bones[i] = {last_joint, limb_end};
        last_joint = limb_end;
    }
}

void Limb::update_straight_line() {
    staright_line_instances[0].p0 = start;
    staright_line_instances[0].p1 = end;
    straight_line.set_lines(staright_line_instances);
}

void Limb::backward_pass() {
    bones.back().p1 = end;
    for (int i = bones.size() - 1; i >= 0; i--) {
        glm::vec3 dir = glm::normalize(bones[i].p0 - bones[i].p1); 
        bones[i].p0 = bones[i].p1 + dir * bone_lengths[i];

        if (i > 0)
            bones[i-1].p1 = bones[i].p0;
    }
    limb_line.set_lines(bones);
}

void Limb::forward_pass() {
    bones.front().p0 = start;
    for (int i = 0; i < bones.size(); i++) {
        glm::vec3 dir = glm::normalize(bones[i].p1 - bones[i].p0); 
        bones[i].p1 = bones[i].p0 + dir * bone_lengths[i];
        if (i + 1 < bones.size())
            bones[i+1].p0 = bones[i].p1;
    }
    limb_line.set_lines(bones);
}

void Limb::draw(RenderState state) {
    state.transform *= get_model_matrix();
    limb_line.draw(state);
}
