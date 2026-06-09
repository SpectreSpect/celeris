#pragma once

#include <cmath>
#include <algorithm>

#include "drawable.h"
#include "transformable.h"
#include "mesh.h"

class Torus : public Transformable, public Drawable {
public:
    Mesh mesh;

    Torus(
        const glm::vec3& position,
        const glm::vec3& scale,
        const glm::vec3& rotation,
        float radius = 1.0f,
        float tube_radius = 0.3f,
        uint32_t detail = 32u,
        glm::vec3 color = {1.0f, 1.0f, 1.0f}
    );
    static Mesh create_torus_mesh(float radius = 1.0f, float tube_radius = 0.3f, uint32_t detail = 32u, glm::vec3 color = {1.0f, 1.0f, 1.0f});
    virtual void draw(RenderState state) override;
};