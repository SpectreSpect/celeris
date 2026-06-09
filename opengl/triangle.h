#pragma once
#include "mesh.h"

class Triangle : public Drawable, public Transformable {
public:
    Mesh* mesh;

    Triangle(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3);
    ~Triangle();
    Mesh* create_triangle_mesh(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3);
    void update_vbo(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 color1, glm::vec3 color2, glm::vec3 color3);
    void draw(RenderState state) override;

    const glm::vec3& v0() const { return this->v0_; }
    const glm::vec3& v1() const { return this->v1_; }
    const glm::vec3& v2() const { return this->v2_; }
    const glm::vec3& color1() const { return this->color1_; }
    const glm::vec3& color2() const { return this->color2_; }
    const glm::vec3& color3() const { return this->color3_; }
private:
    glm::vec3 v0_;
    glm::vec3 v1_;
    glm::vec3 v2_;
    glm::vec3 color1_;
    glm::vec3 color2_;
    glm::vec3 color3_;
};