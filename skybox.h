#pragma once
#include "transformable.h"
#include "drawable.h"
#include "cubemap.h"
#include "mesh.h"

class Skybox : public Drawable {
public:
    const Cubemap* cubemap;
    Mesh* mesh;
    Skybox() = default;
    Skybox(const Cubemap &cubemap);
    // Skybox();
    virtual void draw(RenderState state);
};