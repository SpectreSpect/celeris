#pragma once
#include "mesh.h"

struct alignas(16) CirlceInstance {
    glm::vec4 position_radius = {0, 0, 0, 1};
    glm::vec4 normal = {0, 1, 0, 1};
    glm::vec4 color = {1, 1, 1, 1};
};

class CircleCloud : public Transformable, public Drawable {
public:
    Mesh* mesh;
    VertexLayout* vertex_layout;
    VertexLayout* instance_layout;
    VBO* instance_vbo;
    unsigned int instance_count = 1;
    unsigned int num_indices = 0;

    CircleCloud();
    void set_instances(const std::vector<CirlceInstance>& instances);

    virtual void draw(RenderState state);
};