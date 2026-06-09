#pragma once
#include "vao.h"
#include "vbo.h"
#include "ebo.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstddef> // offsetof
#include "drawable.h"
#include "transformable.h"

struct LineInstance {
    glm::vec3 p0;
    glm::vec3 p1;
};

class Line : public Drawable, public Transformable{
public:
    Line();
    ~Line();

    glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f);
    float width = 3.0f; // pixels

    // call this each frame (or when lines change)
    void set_lines(const std::vector<LineInstance>& lines);

    // draw using your own Program/RenderState (not shown here)
    // void draw_instanced(int instanceCount) const;
    void draw(RenderState state) override;

private:
    VAO* vao = nullptr;
    VBO* quad_vbo = nullptr;      // static: 4 corners
    EBO* quad_ebo = nullptr;      // static: 6 indices
    VBO* instance_vbo = nullptr;  // dynamic: p0,p1 per line
    int instance_count = 0;
};
